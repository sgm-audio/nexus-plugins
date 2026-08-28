#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

class CompressorDSP {
public:
    double sample_rate   = 44100.0;

    double threshold_db  = -18.0;
    double ratio         =   4.0;
    double knee_db       =   3.0;
    double attack_ms     =   5.0;
    double release_ms    = 100.0;
    double makeup_db     =   0.0;
    double lookahead_ms  =   5.0;
    double mix           =   1.0;

    double attack_coeff  = 0.0;
    double release_coeff = 0.0;

    size_t lookahead_samples = 0;
    size_t buf_size          = 0;
    size_t buf_write         = 0;
    std::vector<float> buf[2];

    // Current gain reduction in dB (≤ 0); read by the GUI thread via atomic wrapper in the processor.
    double env_db = 0.0;

    void prepare(double sr)
    {
        sample_rate = sr;
        buf_size    = static_cast<size_t>(sr * 0.05) + 2; // max 50 ms at any rate
        for (int c = 0; c < 2; ++c)
            buf[c].assign(buf_size, 0.0f);
        buf_write = 0;
        env_db    = 0.0;
        update_coeffs();
    }

    void update_coeffs()
    {
        auto ms_to_coeff = [this](double ms) -> double {
            if (ms < 1e-4) return 0.0;
            return std::exp(-1.0 / (ms * sample_rate / 1000.0));
        };
        attack_coeff  = ms_to_coeff(attack_ms);
        release_coeff = ms_to_coeff(release_ms);

        lookahead_samples = static_cast<size_t>(lookahead_ms * sample_rate / 1000.0);
        if (lookahead_samples >= buf_size) lookahead_samples = buf_size - 1;
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) std::fill(buf[c].begin(), buf[c].end(), 0.0f);
        buf_write = 0;
        env_db    = 0.0;
    }

    // Soft-knee gain computer — returns gain reduction in dB (≤ 0)
    double gain_computer(double level_db) const
    {
        const double excess    = level_db - threshold_db;
        const double half_knee = knee_db * 0.5;
        const double slope     = 1.0 / ratio - 1.0; // negative

        if (knee_db > 0.0 && excess > -half_knee && excess < half_knee) {
            const double x = (excess + half_knee) / knee_db;
            return slope * knee_db * 0.5 * x * x;
        }
        if (excess >= half_knee)
            return slope * excess;
        return 0.0;
    }

    // Process in-place. in and out may alias (in-place) — safe because we read dry from ring buffer.
    void process(float* const* in, float* const* out, uint32_t n_ch, uint32_t n_frames)
    {
        for (uint32_t i = 0; i < n_frames; ++i) {
            // Write input to lookahead ring buffer
            for (uint32_t c = 0; c < n_ch && c < 2; ++c)
                buf[c][buf_write] = in[c][i];

            // Level detection on un-delayed input (feedforward)
            double peak = 0.0;
            for (uint32_t c = 0; c < n_ch && c < 2; ++c)
                peak = std::max(peak, static_cast<double>(std::abs(in[c][i])));

            const double level_db  = (peak > 1e-9) ? 20.0 * std::log10(peak) : -180.0;
            const double target_gr = gain_computer(level_db);

            // IIR ballistics
            const double coeff = (target_gr < env_db) ? attack_coeff : release_coeff;
            env_db = coeff * env_db + (1.0 - coeff) * target_gr;

            const double total_lin = std::pow(10.0, (env_db + makeup_db) / 20.0);
            const size_t read_pos  = (buf_write + buf_size - lookahead_samples) % buf_size;

            for (uint32_t c = 0; c < n_ch && c < 2; ++c) {
                const float dry = buf[c][read_pos];
                const float wet = dry * static_cast<float>(total_lin);
                out[c][i] = dry + static_cast<float>(mix) * (wet - dry);
            }

            buf_write = (buf_write + 1) % buf_size;
        }
    }
};
