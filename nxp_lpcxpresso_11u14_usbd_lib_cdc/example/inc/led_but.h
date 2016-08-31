#pragma once

void vLedTask(void *pvParameters);
void vButLedTask(void *pvParameters);
void allowButtonsInput();

void setButtonLedDelayNormal();
void setButtonLedDelayFast();

void turnOnRedLed();
void turnOnGreenLed();
void turnOnYellowLed();
void turnOnPreviousLed();
