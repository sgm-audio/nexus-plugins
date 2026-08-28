// MambaCompressor — CLAP plugin
// Lookahead feedforward compressor with soft-knee gain computer and IIR ballistics.
//
// Parameters:
//   0  Threshold   -60..0 dB       default -18
//   1  Ratio       1..20           default 4
//   2  Knee        0..12 dB        default 3
//   3  Attack      0.1..200 ms     default 5
//   4  Release     10..2000 ms     default 100
//   5  Makeup Gain -12..+24 dB     default 0
//   6  Lookahead   0..50 ms        default 5
//   7  Mix         0..1 (dry/wet)  default 1

#include <clap/clap.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

// ============================================================
// Metadata
// ============================================================

static constexpr char kPluginId[]   = "com.sgm-studios.mamba-compressor";
static constexpr char kPluginName[] = "Mamba Compressor";

static const char* const kFeatures[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_COMPRESSOR,
    nullptr,
};

static const clap_plugin_descriptor_t kDescriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id           = kPluginId,
    .name         = kPluginName,
    .vendor       = "SGM Studios",
    .url          = "",
    .manual_url   = "",
    .support_url  = "",
    .version      = "1.0.0",
    .description  = "Context-aware lookahead compressor",
    .features     = kFeatures,
};

// ============================================================
// Parameters
// ============================================================

enum ParamId : clap_id {
    P_THRESHOLD = 0,
    P_RATIO     = 1,
    P_KNEE      = 2,
    P_ATTACK    = 3,
    P_RELEASE   = 4,
    P_MAKEUP    = 5,
    P_LOOKAHEAD = 6,
    P_MIX       = 7,
    P_COUNT     = 8,
};

struct ParamDef {
    clap_id     id;
    const char* name;
    const char* module;
    double      min_val, max_val, default_val;
    clap_param_info_flags flags;
};

static constexpr ParamDef kParams[P_COUNT] = {
    {P_THRESHOLD, "Threshold",   "Dynamics", -60.0, 0.0,    -18.0, CLAP_PARAM_IS_AUTOMATABLE},
    {P_RATIO,     "Ratio",       "Dynamics",  1.0,  20.0,    4.0,  CLAP_PARAM_IS_AUTOMATABLE},
    {P_KNEE,      "Knee",        "Dynamics",  0.0,  12.0,    3.0,  CLAP_PARAM_IS_AUTOMATABLE},
    {P_ATTACK,    "Attack",      "Dynamics",  0.1,  200.0,   5.0,  CLAP_PARAM_IS_AUTOMATABLE},
    {P_RELEASE,   "Release",     "Dynamics", 10.0,  2000.0, 100.0, CLAP_PARAM_IS_AUTOMATABLE},
    {P_MAKEUP,    "Makeup Gain", "Output",   -12.0, 24.0,    0.0,  CLAP_PARAM_IS_AUTOMATABLE},
    {P_LOOKAHEAD, "Lookahead",   "Timing",    0.0,  50.0,    5.0,  CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_REQUIRES_PROCESS},
    {P_MIX,       "Mix",         "Output",    0.0,   1.0,    1.0,  CLAP_PARAM_IS_AUTOMATABLE},
};

// ============================================================
// DSP
// ============================================================

class CompressorDSP {
public:
    double sample_rate = 44100.0;

    // Params (written by audio thread after sync)
    double threshold_db  = -18.0;
    double ratio         =   4.0;
    double knee_db       =   3.0;
    double attack_ms     =   5.0;
    double release_ms    = 100.0;
    double makeup_db     =   0.0;
    double lookahead_ms  =   5.0;
    double mix           =   1.0;

    // Derived
    double attack_coeff  = 0.0;
    double release_coeff = 0.0;

    // Lookahead ring buffer (2 ch)
    size_t lookahead_samples = 0;
    size_t buf_size          = 0;
    size_t buf_write         = 0;
    std::vector<float> buf[2];

    // Ballistics state (one per channel + linked)
    double env_db = 0.0;   // Smoothed gain reduction in dB (≤ 0)

    // -----------------------------------------------------------

    void prepare(double sr) {
        sample_rate = sr;
        // Allocate for max 50 ms at any sample rate
        buf_size = static_cast<size_t>(sr * 0.05) + 2;
        for (int c = 0; c < 2; ++c) {
            buf[c].assign(buf_size, 0.0f);
        }
        buf_write = 0;
        env_db    = 0.0;
        update_coeffs();
    }

    void update_coeffs() {
        auto ms_to_coeff = [this](double ms) -> double {
            if (ms < 1e-4) return 0.0;
            return std::exp(-1.0 / (ms * sample_rate / 1000.0));
        };
        attack_coeff  = ms_to_coeff(attack_ms);
        release_coeff = ms_to_coeff(release_ms);

        lookahead_samples = static_cast<size_t>(lookahead_ms * sample_rate / 1000.0);
        if (lookahead_samples >= buf_size) lookahead_samples = buf_size - 1;
    }

    void reset() {
        for (int c = 0; c < 2; ++c) std::fill(buf[c].begin(), buf[c].end(), 0.0f);
        buf_write = 0;
        env_db    = 0.0;
    }

    // Soft-knee gain computer: returns gain reduction in dB (≤ 0)
    double gain_computer(double level_db) const {
        double excess     = level_db - threshold_db;
        double half_knee  = knee_db * 0.5;
        double slope      = 1.0 / ratio - 1.0;   // negative

        if (knee_db > 0.0 && excess > -half_knee && excess < half_knee) {
            // Soft knee: quadratic blend
            double x = (excess + half_knee) / knee_db;   // 0..1 in knee
            return slope * knee_db * 0.5 * x * x;
        } else if (excess >= half_knee) {
            return slope * excess;
        }
        return 0.0;
    }

    void process(float* const* in, float* const* out,
                 uint32_t n_ch, uint32_t n_frames)
    {
        for (uint32_t i = 0; i < n_frames; ++i) {
            // Write current input to ring buffer
            for (uint32_t c = 0; c < n_ch && c < 2; ++c) {
                buf[c][buf_write] = in[c][i];
            }

            // Compute peak level from current (un-delayed) input
            double peak = 0.0;
            for (uint32_t c = 0; c < n_ch && c < 2; ++c) {
                peak = std::max(peak, static_cast<double>(std::abs(in[c][i])));
            }
            double level_db = (peak > 1e-9) ? 20.0 * std::log10(peak) : -180.0;

            // Gain computer
            double target_gr = gain_computer(level_db);   // ≤ 0

            // Ballistics: attack when gain reduction increases, release when decreasing
            double coeff = (target_gr < env_db) ? attack_coeff : release_coeff;
            env_db = coeff * env_db + (1.0 - coeff) * target_gr;

            // Total gain in linear (gain reduction + makeup)
            double total_lin = std::pow(10.0, (env_db + makeup_db) / 20.0);

            // Read delayed audio from ring buffer
            size_t read_pos = (buf_write + buf_size - lookahead_samples) % buf_size;

            for (uint32_t c = 0; c < n_ch && c < 2; ++c) {
                float dry = buf[c][read_pos];
                float wet = dry * static_cast<float>(total_lin);
                out[c][i] = dry + static_cast<float>(mix) * (wet - dry);
            }

            buf_write = (buf_write + 1) % buf_size;
        }
    }
};

// ============================================================
// Plugin instance
// ============================================================

struct MambaPlugin {
    clap_plugin_t      clap;       // Must be first — we cast plugin_data to this
    const clap_host_t* host;

    std::atomic<double> param_vals[P_COUNT];
    bool latency_changed = false;

    const clap_host_latency_t* host_latency = nullptr;

    CompressorDSP dsp;

    MambaPlugin() {
        for (clap_id i = 0; i < P_COUNT; ++i)
            param_vals[i].store(kParams[i].default_val, std::memory_order_relaxed);
    }

    void sync_to_dsp() {
        dsp.threshold_db = param_vals[P_THRESHOLD].load(std::memory_order_relaxed);
        dsp.ratio        = param_vals[P_RATIO    ].load(std::memory_order_relaxed);
        dsp.knee_db      = param_vals[P_KNEE     ].load(std::memory_order_relaxed);
        dsp.attack_ms    = param_vals[P_ATTACK   ].load(std::memory_order_relaxed);
        dsp.release_ms   = param_vals[P_RELEASE  ].load(std::memory_order_relaxed);
        dsp.makeup_db    = param_vals[P_MAKEUP   ].load(std::memory_order_relaxed);
        dsp.lookahead_ms = param_vals[P_LOOKAHEAD].load(std::memory_order_relaxed);
        dsp.mix          = param_vals[P_MIX      ].load(std::memory_order_relaxed);
        dsp.update_coeffs();
    }
};

static MambaPlugin* self(const clap_plugin_t* p) {
    return static_cast<MambaPlugin*>(p->plugin_data);
}

// ============================================================
// Core plugin callbacks
// ============================================================

static bool on_init(const clap_plugin_t* plugin) {
    auto* s = self(plugin);
    s->host_latency = static_cast<const clap_host_latency_t*>(
        s->host->get_extension(s->host, CLAP_EXT_LATENCY));
    return true;
}

static void on_destroy(const clap_plugin_t* plugin) {
    delete self(plugin);
}

static bool on_activate(const clap_plugin_t* plugin, double sample_rate,
                        uint32_t /*min_frames*/, uint32_t /*max_frames*/) {
    auto* s = self(plugin);
    s->sync_to_dsp();
    s->dsp.prepare(sample_rate);
    return true;
}

static void on_deactivate(const clap_plugin_t*) {}
static bool on_start_processing(const clap_plugin_t*) { return true; }
static void on_stop_processing(const clap_plugin_t*)  {}

static void on_reset(const clap_plugin_t* plugin) {
    self(plugin)->dsp.reset();
}

static clap_process_status on_process(const clap_plugin_t* plugin,
                                      const clap_process_t* proc) {
    auto* s = self(plugin);

    // Consume parameter events
    uint32_t n_ev = proc->in_events->size(proc->in_events);
    for (uint32_t i = 0; i < n_ev; ++i) {
        const clap_event_header_t* hdr = proc->in_events->get(proc->in_events, i);
        if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* pv = reinterpret_cast<const clap_event_param_value_t*>(hdr);
            if (pv->param_id < static_cast<clap_id>(P_COUNT)) {
                s->param_vals[pv->param_id].store(pv->value, std::memory_order_relaxed);
                if (pv->param_id == P_LOOKAHEAD) s->latency_changed = true;
            }
        }
    }

    s->sync_to_dsp();

    // Notify host of latency change (must happen on main thread; request callback)
    if (s->latency_changed) {
        s->host->request_callback(s->host);
    }

    // Guard against missing buffers
    if (!proc->audio_inputs_count || !proc->audio_outputs_count) return CLAP_PROCESS_CONTINUE;

    const auto& in  = proc->audio_inputs[0];
    const auto& out = proc->audio_outputs[0];
    uint32_t n_ch   = std::min(in.channel_count, out.channel_count);

    if (n_ch == 0 || !in.data32 || !out.data32) return CLAP_PROCESS_CONTINUE;

    // Build channel pointer arrays (up to stereo)
    float* ch_in[2]  = {nullptr, nullptr};
    float* ch_out[2] = {nullptr, nullptr};
    for (uint32_t c = 0; c < n_ch && c < 2; ++c) {
        ch_in[c]  = in.data32[c];
        ch_out[c] = out.data32[c];
        if (!ch_in[c] || !ch_out[c]) { n_ch = c; break; }
    }

    if (n_ch > 0)
        s->dsp.process(ch_in, ch_out, n_ch, proc->frames_count);

    return CLAP_PROCESS_CONTINUE;
}

static void on_main_thread(const clap_plugin_t* plugin) {
    auto* s = self(plugin);
    if (s->latency_changed && s->host_latency) {
        s->host_latency->changed(s->host);
        s->latency_changed = false;
    }
}

// ============================================================
// Extension: Audio Ports
// ============================================================

static uint32_t ap_count(const clap_plugin_t*, bool) { return 1; }

static bool ap_get(const clap_plugin_t*, uint32_t index,
                   bool is_input, clap_audio_port_info_t* info) {
    if (index != 0) return false;
    info->id            = 0;
    info->channel_count = 2;
    info->flags         = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type     = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    std::snprintf(info->name, sizeof(info->name), "%s", is_input ? "Input" : "Output");
    return true;
}

static const clap_plugin_audio_ports_t kExtAudioPorts = {
    .count = ap_count,
    .get   = ap_get,
};

// ============================================================
// Extension: Parameters
// ============================================================

static uint32_t par_count(const clap_plugin_t*) { return P_COUNT; }

static bool par_get_info(const clap_plugin_t*, uint32_t idx,
                         clap_param_info_t* info) {
    if (idx >= P_COUNT) return false;
    const auto& p   = kParams[idx];
    info->id         = p.id;
    info->flags      = p.flags;
    info->min_value  = p.min_val;
    info->max_value  = p.max_val;
    info->default_value = p.default_val;
    info->cookie     = nullptr;
    std::snprintf(info->name,   sizeof(info->name),   "%s", p.name);
    std::snprintf(info->module, sizeof(info->module), "%s", p.module);
    return true;
}

static bool par_get_value(const clap_plugin_t* plugin,
                          clap_id id, double* out) {
    if (id >= P_COUNT) return false;
    *out = self(plugin)->param_vals[id].load(std::memory_order_relaxed);
    return true;
}

static bool par_to_text(const clap_plugin_t*, clap_id id,
                        double val, char* buf, uint32_t cap) {
    if (id >= P_COUNT) return false;
    switch (id) {
        case P_THRESHOLD: std::snprintf(buf, cap, "%.1f dB", val);    break;
        case P_RATIO:     std::snprintf(buf, cap, "%.2f:1", val);     break;
        case P_KNEE:      std::snprintf(buf, cap, "%.1f dB", val);    break;
        case P_ATTACK:    std::snprintf(buf, cap, "%.2f ms", val);    break;
        case P_RELEASE:   std::snprintf(buf, cap, "%.1f ms", val);    break;
        case P_MAKEUP:    std::snprintf(buf, cap, "%.1f dB", val);    break;
        case P_LOOKAHEAD: std::snprintf(buf, cap, "%.2f ms", val);    break;
        case P_MIX:       std::snprintf(buf, cap, "%.0f%%", val*100.0); break;
        default:          std::snprintf(buf, cap, "%.2f", val);
    }
    return true;
}

static bool par_from_text(const clap_plugin_t*, clap_id,
                          const char* text, double* out) {
    char* end = nullptr;
    *out = std::strtod(text, &end);
    return end != text;
}

static void par_flush(const clap_plugin_t* plugin,
                      const clap_input_events_t* in,
                      const clap_output_events_t*) {
    auto* s   = self(plugin);
    uint32_t n = in->size(in);
    for (uint32_t i = 0; i < n; ++i) {
        const clap_event_header_t* hdr = in->get(in, i);
        if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* pv = reinterpret_cast<const clap_event_param_value_t*>(hdr);
            if (pv->param_id < static_cast<clap_id>(P_COUNT)) {
                s->param_vals[pv->param_id].store(pv->value, std::memory_order_relaxed);
                // par_flush runs on the main thread — notify host directly if latency changed
                if (pv->param_id == P_LOOKAHEAD && s->host_latency)
                    s->host_latency->changed(s->host);
            }
        }
    }
}

static const clap_plugin_params_t kExtParams = {
    .count          = par_count,
    .get_info       = par_get_info,
    .get_value      = par_get_value,
    .value_to_text  = par_to_text,
    .text_to_value  = par_from_text,
    .flush          = par_flush,
};

// ============================================================
// Extension: Latency
// ============================================================

static uint32_t lat_get(const clap_plugin_t* plugin) {
    return static_cast<uint32_t>(self(plugin)->dsp.lookahead_samples);
}

static const clap_plugin_latency_t kExtLatency = { .get = lat_get };

// ============================================================
// Extension: State (preset save / load)
// ============================================================

static bool state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    auto* s = self(plugin);
    double buf[P_COUNT];
    for (clap_id i = 0; i < P_COUNT; ++i)
        buf[i] = s->param_vals[i].load(std::memory_order_relaxed);
    int64_t bytes = static_cast<int64_t>(sizeof(buf));
    return stream->write(stream, buf, bytes) == bytes;
}

static bool state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) {
    auto* s = self(plugin);
    double buf[P_COUNT];
    int64_t bytes = static_cast<int64_t>(sizeof(buf));
    if (stream->read(stream, buf, bytes) != bytes) return false;
    for (clap_id i = 0; i < P_COUNT; ++i) {
        double v = std::clamp(buf[i], kParams[i].min_val, kParams[i].max_val);
        s->param_vals[i].store(v, std::memory_order_relaxed);
    }
    return true;
}

static const clap_plugin_state_t kExtState = {
    .save = state_save,
    .load = state_load,
};

// ============================================================
// get_extension dispatch
// ============================================================

static const void* on_get_extension(const clap_plugin_t*, const char* id) {
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &kExtAudioPorts;
    if (!std::strcmp(id, CLAP_EXT_PARAMS))      return &kExtParams;
    if (!std::strcmp(id, CLAP_EXT_LATENCY))     return &kExtLatency;
    if (!std::strcmp(id, CLAP_EXT_STATE))       return &kExtState;
    return nullptr;
}

// ============================================================
// Factory
// ============================================================

static uint32_t fac_count(const clap_plugin_factory_t*) { return 1; }

static const clap_plugin_descriptor_t* fac_descriptor(
    const clap_plugin_factory_t*, uint32_t idx) {
    return idx == 0 ? &kDescriptor : nullptr;
}

static const clap_plugin_t* fac_create(const clap_plugin_factory_t*,
                                       const clap_host_t* host,
                                       const char* plugin_id) {
    if (std::strcmp(plugin_id, kPluginId) != 0) return nullptr;

    auto* p    = new MambaPlugin();
    p->host    = host;
    p->clap    = {
        .desc             = &kDescriptor,
        .plugin_data      = p,
        .init             = on_init,
        .destroy          = on_destroy,
        .activate         = on_activate,
        .deactivate       = on_deactivate,
        .start_processing = on_start_processing,
        .stop_processing  = on_stop_processing,
        .reset            = on_reset,
        .process          = on_process,
        .get_extension    = on_get_extension,
        .on_main_thread   = on_main_thread,
    };
    return &p->clap;
}

static const clap_plugin_factory_t kFactory = {
    .get_plugin_count      = fac_count,
    .get_plugin_descriptor = fac_descriptor,
    .create_plugin         = fac_create,
};

// ============================================================
// Entry point (exported symbol DAWs scan for)
// ============================================================

static bool entry_init(const char*) { return true; }
static void entry_deinit()          {}

static const void* entry_get_factory(const char* id) {
    if (!std::strcmp(id, CLAP_PLUGIN_FACTORY_ID)) return &kFactory;
    return nullptr;
}

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,
    .init         = entry_init,
    .deinit       = entry_deinit,
    .get_factory  = entry_get_factory,
};
