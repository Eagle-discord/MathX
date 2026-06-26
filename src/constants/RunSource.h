#pragma once
enum class RunSource {
	User /*Run by the user, when they press enter in the QLineEdit*/,
	App /*used internally to send expressions back into the start of the execution pipeline, inheriting a lot of support at a low cost*/
};
