#include "SimpleAudio.h"
#include <QFile>
#include <QDebug>

#ifdef _WIN32
    #include <windows.h>
    #include <mmsystem.h>
    #pragma comment(lib, "winmm.lib")
#elif defined(__linux__)
    #include <alsa/asoundlib.h>
    #include <QThread>
    #include <QMutex>
    #include <memory>
#endif

class SimpleAudio::SimpleAudioPrivate
{
public:
    SimpleAudioPrivate() : volume(0.8f), audioData(nullptr), audioSize(0), minimumInterval(50) {}
    ~SimpleAudioPrivate() {
        cleanup();
    }
    
    void cleanup() {
#ifdef _WIN32
        if (audioData) {
            delete[] audioData;
            audioData = nullptr;
            audioSize = 0;
        }
#elif defined(__linux__)
        if (audioData) {
            delete[] audioData;
            audioData = nullptr;
            audioSize = 0;
        }
#endif
    }
    
    float volume;
    char* audioData;
    size_t audioSize;
    int minimumInterval; // Minimum interval between sounds in milliseconds
    
#ifdef __linux__
    // WAV format info
    int sampleRate;
    int channels;
    int bitsPerSample;
    size_t dataOffset;
#endif
};

SimpleAudio::SimpleAudio() : d(new SimpleAudioPrivate())
{
}

SimpleAudio::~SimpleAudio()
{
    delete d;
}

bool SimpleAudio::loadWavFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "SimpleAudio: Could not open file:" << filePath;
        return false;
    }
    
    // Read the entire file
    QByteArray fileData = file.readAll();
    file.close();
    
    if (fileData.size() < 44) {
        qWarning() << "SimpleAudio: File too small to be a valid WAV file";
        return false;
    }
    
    // Basic WAV header validation
    if (fileData.left(4) != "RIFF" || fileData.mid(8, 4) != "WAVE") {
        qWarning() << "SimpleAudio: Not a valid WAV file";
        return false;
    }
    
    d->cleanup();
    
#ifdef _WIN32
    // On Windows, we can use PlaySound with memory data
    d->audioSize = fileData.size();
    d->audioData = new char[d->audioSize];
    memcpy(d->audioData, fileData.data(), d->audioSize);
    return true;
    
#elif defined(__linux__)
    // Parse WAV header for Linux ALSA playback
    const char* data = fileData.constData();
    
    // Find the "fmt " chunk
    size_t pos = 12;
    while (pos < fileData.size() - 8) {
        if (memcmp(data + pos, "fmt ", 4) == 0) {
            uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(data + pos + 4);
            if (chunkSize >= 16) {
                uint16_t audioFormat = *reinterpret_cast<const uint16_t*>(data + pos + 8);
                if (audioFormat != 1) { // PCM
                    qWarning() << "SimpleAudio: Only PCM format supported";
                    return false;
                }
                
                d->channels = *reinterpret_cast<const uint16_t*>(data + pos + 10);
                d->sampleRate = *reinterpret_cast<const uint32_t*>(data + pos + 12);
                d->bitsPerSample = *reinterpret_cast<const uint16_t*>(data + pos + 22);
                
                if (d->bitsPerSample != 16) {
                    qWarning() << "SimpleAudio: Only 16-bit samples supported";
                    return false;
                }
                break;
            }
        }
        pos += 8 + *reinterpret_cast<const uint32_t*>(data + pos + 4);
    }
    
    // Find the "data" chunk
    pos = 12;
    while (pos < fileData.size() - 8) {
        if (memcmp(data + pos, "data", 4) == 0) {
            uint32_t dataSize = *reinterpret_cast<const uint32_t*>(data + pos + 4);
            d->dataOffset = pos + 8;
            d->audioSize = dataSize;
            d->audioData = new char[d->audioSize];
            memcpy(d->audioData, data + d->dataOffset, d->audioSize);
            return true;
        }
        pos += 8 + *reinterpret_cast<const uint32_t*>(data + pos + 4);
    }
    
    qWarning() << "SimpleAudio: Could not find data chunk in WAV file";
    return false;
    
#else
    qWarning() << "SimpleAudio: Platform not supported";
    return false;
#endif
}

void SimpleAudio::play()
{
    if (!d->audioData || d->audioSize == 0) {
        return;
    }
    
#ifdef _WIN32
    // Rate limiting: Don't play if we just played recently (prevents CPU spikes)
    static DWORD lastPlayTime = 0;
    DWORD currentTime = GetTickCount();
    DWORD minInterval = static_cast<DWORD>(d->minimumInterval);
    
    if (currentTime - lastPlayTime < minInterval) {
        return; // Skip this play request to prevent audio spam
    }
    lastPlayTime = currentTime;
    
    // Stop any currently playing sound first to prevent overlapping
    PlaySoundA(nullptr, nullptr, SND_PURGE);
    
    // Use optimized flags for better performance
    DWORD flags = SND_MEMORY | SND_ASYNC | SND_NODEFAULT | SND_NOWAIT;
    PlaySoundA(d->audioData, nullptr, flags);
    
#elif defined(__linux__)
    // Use ALSA for Linux playback in a separate thread
    QThread::create([this]() {
        snd_pcm_t* pcm_handle;
        int err;
        
        // Open the PCM device
        err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if (err < 0) {
            // Fallback: try to use system beep
            system("paplay /usr/share/sounds/alsa/Front_Left.wav 2>/dev/null || aplay /usr/share/sounds/alsa/Front_Left.wav 2>/dev/null || echo -e '\\a'");
            return;
        }
        
        // Set parameters
        snd_pcm_hw_params_t* hw_params;
        snd_pcm_hw_params_malloc(&hw_params);
        snd_pcm_hw_params_any(pcm_handle, hw_params);
        snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
        snd_pcm_hw_params_set_channels(pcm_handle, hw_params, d->channels);
        snd_pcm_hw_params_set_rate(pcm_handle, hw_params, d->sampleRate, 0);
        
        err = snd_pcm_hw_params(pcm_handle, hw_params);
        if (err < 0) {
            snd_pcm_hw_params_free(hw_params);
            snd_pcm_close(pcm_handle);
            return;
        }
        
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_prepare(pcm_handle);
        
        // Play the audio data
        snd_pcm_sframes_t frames = d->audioSize / (d->channels * 2); // 16-bit = 2 bytes per sample
        snd_pcm_writei(pcm_handle, d->audioData, frames);
        snd_pcm_drain(pcm_handle);
        snd_pcm_close(pcm_handle);
    })->start();
#endif
}

void SimpleAudio::setVolume(float volume)
{
    d->volume = qBound(0.0f, volume, 1.0f);
    // Note: Volume control would require more complex implementation
    // For simplicity, we'll just store the value but not apply it
}

void SimpleAudio::setMinimumInterval(int milliseconds)
{
    d->minimumInterval = qBound(10, milliseconds, 1000); // Between 10ms and 1000ms
}

bool SimpleAudio::isAudioAvailable()
{
#ifdef _WIN32
    return true; // Windows always has PlaySound
#elif defined(__linux__)
    // Check if ALSA is available
    snd_pcm_t* pcm_handle;
    int err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (err >= 0) {
        snd_pcm_close(pcm_handle);
        return true;
    }
    return false; // Fallback to system beep will still work
#else
    return false;
#endif
}
