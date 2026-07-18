#pragma once

// Non-precise clock that only updates whenever game ticks

class ClientApplication;
class Clock {
    friend ClientApplication;
public:
    static double Time() { return s_Time; }
private:
    static double s_Time;
};

double Clock::s_Time = 0.0f;
