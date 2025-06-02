#include "SDLControllerManager.h"
#include <QtMath>
#include <QDebug>

SDLControllerManager::SDLControllerManager(QObject *parent)
    : QObject(parent), pollTimer(new QTimer(this)) {

    connect(pollTimer, &QTimer::timeout, this, [=]() {
        SDL_PumpEvents();  // ✅ Update SDL input state

        // 🔁 Poll for axis movement (left stick)
        if (controller) {
            int x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
            int y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

            const int DEADZONE = 16000;
            if (qAbs(x) >= DEADZONE || qAbs(y) >= DEADZONE) {
                float fx = x / 32768.0f;
                float fy = y / 32768.0f;

                float angle = qAtan2(-fy, fx) * 180.0 / M_PI;
                if (angle < 0) angle += 360;

                int angleInt = static_cast<int>(angle);

                // ✅ Invert angle for dial so clockwise = clockwise
                angleInt = (360 - angleInt) % 360;


                if (qAbs(angleInt - lastAngle) > 3) {
                    lastAngle = angleInt;
                    emit leftStickAngleChanged(angleInt);
                }
                leftStickActive = true;  // ✅ Mark stick as active
            }
            else {
                // Stick is in deadzone (center)
                if (leftStickActive) {
                    // ✅ Was active → now released → emit release signal once
                    emit leftStickReleased();
                    leftStickActive = false;
                }
            }
        }

        SDL_PumpEvents();

        // Check hold timer
        for (auto it = buttonPressTime.begin(); it != buttonPressTime.end(); ++it) {
            QString btn = it.key();
            quint32 pressTime = it.value();
            quint32 now = SDL_GetTicks();

            if (!buttonHeldEmitted.value(btn, false) && (now - pressTime) >= HOLD_THRESHOLD) {
                emit buttonHeld(btn);
                buttonHeldEmitted[btn] = true;
            }
        }

        // 🔁 Poll for button events
        SDL_Event e;

        /*
        for (int i = SDL_CONTROLLER_BUTTON_A; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
            SDL_GameControllerButton button = static_cast<SDL_GameControllerButton>(i);
            const char* name = SDL_GameControllerGetStringForButton(button);
            if (SDL_GameControllerGetButton(controller, button)) {
                qDebug() << "Button" << name << "is pressed";
            }
        }
        This is for testing button mappings. 
        */

        

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                QString btnName = getButtonName(e.cbutton.button);
                buttonPressTime[btnName] = SDL_GetTicks();
                buttonHeldEmitted[btnName] = false;
            }
        
            if (e.type == SDL_CONTROLLERBUTTONUP) {
                QString btnName = getButtonName(e.cbutton.button);
        
                quint32 pressTime = buttonPressTime.value(btnName, 0);
                quint32 now = SDL_GetTicks();
                quint32 duration = now - pressTime;
        
                if (duration < HOLD_THRESHOLD) {
                    emit buttonSinglePress(btnName);
                } else {
                    emit buttonReleased(btnName);
                }
        
                buttonPressTime.remove(btnName);
                buttonHeldEmitted.remove(btnName);
            }
        }
    });
}

QString SDLControllerManager::getButtonName(Uint8 sdlButton) {
    switch (sdlButton) {
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LEFTSHOULDER";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RIGHTSHOULDER";
        case SDL_CONTROLLER_BUTTON_PADDLE2: return "PADDLE2";
        case SDL_CONTROLLER_BUTTON_PADDLE4: return "PADDLE4";
        case SDL_CONTROLLER_BUTTON_Y: return "Y";
        case SDL_CONTROLLER_BUTTON_A: return "A";
        case SDL_CONTROLLER_BUTTON_B: return "B";
        case SDL_CONTROLLER_BUTTON_X: return "X";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "LEFTSTICK";
        case SDL_CONTROLLER_BUTTON_START: return "START";
        case SDL_CONTROLLER_BUTTON_GUIDE: return "GUIDE";
    }
    return "UNKNOWN";
}


SDLControllerManager::~SDLControllerManager() {
    if (controller) SDL_GameControllerClose(controller);
    if (sdlInitialized) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void SDLControllerManager::start() {
    if (!sdlInitialized) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
            qWarning() << "Failed to initialize SDL controller subsystem";
            return;
        }
        sdlInitialized = true;
    }

    SDL_GameControllerEventState(SDL_ENABLE);  // ✅ Enable controller events

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) {
                qDebug() << "Controller added!";
                break;
            }
        }
    }

    pollTimer->start(16); // 60 FPS polling
}

void SDLControllerManager::stop() {
    pollTimer->stop();
}
