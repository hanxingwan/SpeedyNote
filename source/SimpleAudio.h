#ifndef SIMPLEAUDIO_H
#define SIMPLEAUDIO_H

#include <QString>

/**
 * Simple, lightweight cross-platform audio playback for WAV files
 * Replaces Qt6 Multimedia to reduce dependencies and load time
 */
class SimpleAudio
{
public:
    SimpleAudio();
    ~SimpleAudio();
    
    // Load a WAV file from the given path
    bool loadWavFile(const QString& filePath);
    
    // Play the loaded audio (non-blocking)
    void play();
    
    // Set volume (0.0 to 1.0)
    void setVolume(float volume);
    
    // Check if audio is available on this system
    static bool isAudioAvailable();
    
private:
    class SimpleAudioPrivate;
    SimpleAudioPrivate* d;
};

#endif // SIMPLEAUDIO_H
