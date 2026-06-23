#pragma once
#include "RunSource.h"
#include "RunState.h"

namespace StateUtils {
    //setters
	void setRunSource(RunSource runSource);
    void setRunState(RunState runState);

    //getters
    RunState getRunState();
    RunSource getRunSource();

}





class AppState
{
private:
    RunSource m_runSource = RunSource::User;
    RunState m_runState;
    AppState() = default;

public:
    static AppState& instance();

    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;

    void setRunSource(RunSource runSource);
    RunSource getRunSource() const;

    void setRunState(RunState runSource);
    RunState getRunState() const;

};