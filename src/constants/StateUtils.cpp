#include "StateUtils.h"

RunSource StateUtils::getRunSource()
{
	return AppState::instance().getRunSource();
}

void StateUtils::setRunSource(RunSource runSource) {
	AppState::instance().setRunSource(runSource);
}



RunState StateUtils::getRunState() {
	return AppState::instance().getRunState();
}

void StateUtils::setRunState(RunState runState) {
	AppState::instance().setRunState(runState);
}

RunSource AppState::getRunSource() const
{
	return m_runSource;
}

RunState AppState::getRunState() const
{
	return m_runState;
}

void AppState::setRunSource(RunSource runState) {
	m_runSource = runState;
}

void AppState::setRunState(RunState runState) {
	m_runState = runState;
}

AppState& AppState::instance()
{
	static AppState s_instance;
	return s_instance;
}