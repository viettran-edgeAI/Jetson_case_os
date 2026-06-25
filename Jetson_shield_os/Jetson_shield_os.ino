#include "system_controller.h"

SystemController gSystemController;

namespace {

const char* stateName(jetson_cfg::SystemState state) {
	switch (state) {
		case jetson_cfg::SystemState::POWER_OFF:
			return "POWER_OFF";
		case jetson_cfg::SystemState::BOOTING_ON:
			return "BOOTING_ON";
		case jetson_cfg::SystemState::RUNNING:
			return "RUNNING";
		case jetson_cfg::SystemState::SHUTTING_DOWN:
			return "SHUTTING_DOWN";
		default:
			return "UNKNOWN";
	}
}

} // namespace

void setup() {
	randomSeed(esp_random());
	gSystemController.init();
	gSystemController.start();
}

void loop() {
	static jetson_cfg::SystemState lastState = jetson_cfg::SystemState::POWER_OFF;
	static uint32_t lastReportMs = 0;

	const jetson_cfg::SystemState currentState = gSystemController.getState();
	const uint32_t now = millis();

	if (currentState != lastState || (now - lastReportMs) >= 5000U) {
		Serial.print("[JSOS] state=");
		Serial.println(stateName(currentState));
		Serial.print("[JSOS] queueDrops=");
		Serial.print(gSystemController.getQueueDropCount());
		Serial.print(" rejectedTransitions=");
		Serial.println(gSystemController.getRejectedTransitionCount());
		lastState = currentState;
		lastReportMs = now;
	}

	delay(50);
}