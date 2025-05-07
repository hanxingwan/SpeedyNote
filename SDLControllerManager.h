#ifndef SDLCONTROLLERMANAGER_H
#define SDLCONTROLLERMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QThread>
#include <QTimer>
#include <SDL2/SDL.h>

class SDLControllerManager : public QObject {
    Q_OBJECT
public:
    explicit SDLControllerManager(QObject *parent = nullptr);
    ~SDLControllerManager();
    SDL_GameController* getController() const { return controller; }

signals:
    void buttonHeld(QString buttonName);
    void buttonReleased(QString buttonName);
    void buttonSinglePress(QString buttonName);
    void leftStickAngleChanged(int angle);
    void leftStickReleased();

public slots:
    void start();
    void stop();

private:
    QTimer *pollTimer;
    SDL_GameController *controller = nullptr;
    bool sdlInitialized = false;
    bool leftStickActive = false;  // Whether stick is out of deadzone


    int lastAngle = -1;
    QString getButtonName(Uint8 sdlButton);

    QMap<QString, quint32> buttonPressTime;
    QMap<QString, bool> buttonHeldEmitted;  // NEW -> whether buttonHeld has been fired
    
    const quint32 HOLD_THRESHOLD = 300;  // ms
    const quint32 POLL_INTERVAL = 16;    // ms
};

#endif // SDLCONTROLLERMANAGER_H
