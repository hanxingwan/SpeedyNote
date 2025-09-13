#include "SimpleAudio.h"
#include <QFile>
#include <QDebug>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
    #include <dsound.h>
    #pragma comment(lib, "dsound.lib")
    #pragma comment(lib, "dxguid.lib")
#elif defined(__linux__)
    #include <alsa/asoundlib.h>
    #include <QThread>
    #include <QMutex>
    #include <memory>
#endif

class SimpleAudio::SimpleAudioPrivate
{
public:
    SimpleAudioPrivate() : volume(0.8f), audioData(nullptr), audioSize(0), minimumInterval(50) {
#ifdef _WIN32
        directSound = nullptr;
        primaryBuffer = nullptr;
        soundBuffer = nullptr;
        directSoundInitialized = false;
        memset(&waveFormat, 0, sizeof(WAVEFORMATEX));
        sampleRate = 44100;
        channels = 1;
        bitsPerSample = 16;
#endif
    }
    ~SimpleAudioPrivate() {
        cleanup();
    }
    
    void cleanup() {
#ifdef _WIN32
        // Clean up DirectSound objects
        if (soundBuffer) {
            soundBuffer->Release();
            soundBuffer = nullptr;
        }
        if (primaryBuffer) {
            primaryBuffer->Release();
            primaryBuffer = nullptr;
        }
        if (directSound) {
            directSound->Release();
            directSound = nullptr;
        }
        directSoundInitialized = false;
        
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
    
#ifdef _WIN32
    bool initializeDirectSound() {
        if (directSoundInitialized) return true;
        
        // Create DirectSound object
        HRESULT hr = DirectSoundCreate8(NULL, &directSound, NULL);
        if (FAILED(hr)) {
            qWarning() << "SimpleAudio: Failed to create DirectSound object, HRESULT:" << QString::number(hr, 16);
            return false;
        }
        
        
        // Set cooperative level - use DSSCL_NORMAL for better compatibility
        HWND hwnd = GetDesktopWindow();
        hr = directSound->SetCooperativeLevel(hwnd, DSSCL_NORMAL);
        if (FAILED(hr)) {
            qWarning() << "SimpleAudio: Failed to set DirectSound cooperative level, HRESULT:" << QString::number(hr, 16);
            // Try with DSSCL_PRIORITY as fallback
            hr = directSound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);
            if (FAILED(hr)) {
                qWarning() << "SimpleAudio: Failed to set DirectSound cooperative level (priority), HRESULT:" << QString::number(hr, 16);
                directSound->Release();
                directSound = nullptr;
                return false;
            }
        }
        
        
        // Create primary buffer to ensure proper setup
        DSBUFFERDESC primaryDesc;
        memset(&primaryDesc, 0, sizeof(DSBUFFERDESC));
        primaryDesc.dwSize = sizeof(DSBUFFERDESC);
        primaryDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
        primaryDesc.dwBufferBytes = 0;
        primaryDesc.lpwfxFormat = NULL;
        
        hr = directSound->CreateSoundBuffer(&primaryDesc, &primaryBuffer, NULL);
        // Continue regardless of primary buffer creation result
        directSoundInitialized = true;
        return true;
    }
    
    bool createSoundBuffer() {
        if (!directSoundInitialized || !audioData) return false;
        
        // Release existing buffer
        if (soundBuffer) {
            soundBuffer->Release();
            soundBuffer = nullptr;
        }
        
        // Set up wave format using actual WAV file format
        waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        waveFormat.nChannels = channels;
        waveFormat.nSamplesPerSec = sampleRate;
        waveFormat.wBitsPerSample = bitsPerSample;
        waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
        waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
        waveFormat.cbSize = 0;
        
        
        // Create buffer description
        DSBUFFERDESC bufferDesc;
        memset(&bufferDesc, 0, sizeof(DSBUFFERDESC));
        bufferDesc.dwSize = sizeof(DSBUFFERDESC);
        bufferDesc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_LOCSOFTWARE | DSBCAPS_GLOBALFOCUS;
        bufferDesc.dwBufferBytes = audioSize;
        bufferDesc.lpwfxFormat = &waveFormat;
        
        
        // Create the sound buffer
        HRESULT hr = directSound->CreateSoundBuffer(&bufferDesc, &soundBuffer, NULL);
        if (FAILED(hr)) {
            qWarning() << "SimpleAudio: Failed to create DirectSound buffer, HRESULT:" << QString::number(hr, 16);
            return false;
        }
        
        // Lock the buffer and copy audio data
        LPVOID audioPtr1, audioPtr2;
        DWORD audioBytes1, audioBytes2;
        
        hr = soundBuffer->Lock(0, audioSize, &audioPtr1, &audioBytes1, &audioPtr2, &audioBytes2, DSBLOCK_ENTIREBUFFER);
        if (FAILED(hr)) {
            qWarning() << "SimpleAudio: Failed to lock DirectSound buffer, HRESULT:" << QString::number(hr, 16);
            return false;
        }
        
        // Copy audio data
        memcpy(audioPtr1, audioData, audioBytes1);
        if (audioPtr2 && audioBytes2 > 0) {
            memcpy(audioPtr2, (char*)audioData + audioBytes1, audioBytes2);
        }
        
        // Unlock the buffer
        hr = soundBuffer->Unlock(audioPtr1, audioBytes1, audioPtr2, audioBytes2);
        if (FAILED(hr)) {
            qWarning() << "SimpleAudio: Failed to unlock DirectSound buffer";
            return false;
        }
        
        return true;
    }
#endif
    
    float volume;
    char* audioData;
    size_t audioSize;
    int minimumInterval; // Minimum interval between sounds in milliseconds
    
#ifdef _WIN32
    // DirectSound objects
    LPDIRECTSOUND8 directSound;
    LPDIRECTSOUNDBUFFER primaryBuffer;
    LPDIRECTSOUNDBUFFER soundBuffer;
    WAVEFORMATEX waveFormat;
    bool directSoundInitialized;
    
    // WAV format info (needed for Windows too)
    int sampleRate;
    int channels;
    int bitsPerSample;
#endif
    
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
    // For DirectSound, we need to parse the WAV file format and extract PCM data
    const char* data = fileData.constData();
    
    // First, find the "fmt " chunk to get format information
    size_t pos = 12;
    bool formatFound = false;
    while (pos < fileData.size() - 8 && !formatFound) {
        if (memcmp(data + pos, "fmt ", 4) == 0) {
            uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(data + pos + 4);
            if (chunkSize >= 16) {
                uint16_t audioFormat = *reinterpret_cast<const uint16_t*>(data + pos + 8);
                if (audioFormat != 1) { // PCM
                    qWarning() << "SimpleAudio: Only PCM format supported, got format:" << audioFormat;
                    return false;
                }
                
                d->channels = *reinterpret_cast<const uint16_t*>(data + pos + 10);
                d->sampleRate = *reinterpret_cast<const uint32_t*>(data + pos + 12);
                d->bitsPerSample = *reinterpret_cast<const uint16_t*>(data + pos + 22);
                
                if (d->bitsPerSample != 16) {
                    qWarning() << "SimpleAudio: Only 16-bit samples supported, got:" << d->bitsPerSample;
                    return false;
                }
                formatFound = true;
            }
        }
        if (!formatFound) {
            pos += 8 + *reinterpret_cast<const uint32_t*>(data + pos + 4);
        }
    }
    
    if (!formatFound) {
        qWarning() << "SimpleAudio: Could not find fmt chunk in WAV file";
        return false;
    }
    
    // Now find the "data" chunk to get the actual PCM audio data
    pos = 12;
    while (pos < fileData.size() - 8) {
        if (memcmp(data + pos, "data", 4) == 0) {
            uint32_t dataSize = *reinterpret_cast<const uint32_t*>(data + pos + 4);
            size_t dataOffset = pos + 8;
            
            // Store only the PCM data (not the WAV header)
            d->audioSize = dataSize;
            d->audioData = new char[d->audioSize];
            memcpy(d->audioData, data + dataOffset, d->audioSize);
            
            // Initialize DirectSound and create buffer
            if (!d->initializeDirectSound()) {
                return false;
            }
            
            if (!d->createSoundBuffer()) {
                return false;
            }
            
            // Apply the current volume setting to the new buffer
            // This ensures volume is set immediately after buffer creation
            float currentVolume = d->volume;
            d->volume = -1.0f; // Force volume update
            setVolume(currentVolume);
            
            return true;
        }
        pos += 8 + *reinterpret_cast<const uint32_t*>(data + pos + 4);
    }
    
    qWarning() << "SimpleAudio: Could not find data chunk in WAV file";
    return false;
    
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
    // DirectSound can handle rapid playback much better than PlaySound
    static DWORD lastPlayTime = 0;
    DWORD currentTime = GetTickCount();
    
    // Apply minimum interval rate limiting
    if (currentTime - lastPlayTime < static_cast<DWORD>(d->minimumInterval)) {
        return;
    }
    lastPlayTime = currentTime;
    
    if (!d->soundBuffer) {
        return;
    }
    
    // Check if the buffer is currently playing and stop it
    DWORD status;
    HRESULT hr = d->soundBuffer->GetStatus(&status);
    if (SUCCEEDED(hr) && (status & DSBSTATUS_PLAYING)) {
        d->soundBuffer->Stop();
    }
    
    // Reset the play position to the beginning
    hr = d->soundBuffer->SetCurrentPosition(0);
    if (FAILED(hr)) {
        return;
    }
    
    // Start playing
    d->soundBuffer->Play(0, 0, 0); // Play once, no looping
    
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
    
#ifdef _WIN32
    // Apply volume to DirectSound buffer if available
    if (d->soundBuffer) {
        // DirectSound volume is in hundredths of decibels (centibels)
        // Range: DSBVOLUME_MIN (-10000) to DSBVOLUME_MAX (0)
        // 0 = full volume, -10000 = silence
        LONG dsVolume;
        if (d->volume <= 0.0f) {
            dsVolume = DSBVOLUME_MIN; // Silence
        } else {
            // Just use full volume (0) and let the system volume control it
            // This eliminates the complexity of DirectSound volume conversion
            dsVolume = DSBVOLUME_MAX; // Always full volume (0 decibels)
        }
        
        d->soundBuffer->SetVolume(dsVolume);
    }
#endif
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
