
#include <M5GFX.h>
#include <LGFX_8BIT_CVBS.h>

static LGFX_8BIT_CVBS display;

void setup() {
  display.init();
}

void loop() {
  display.setCursor(0, 0);
  display.println("Hello World");
  display.display();
}
