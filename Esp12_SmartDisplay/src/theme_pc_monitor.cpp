#include <Arduino.h>

#include "app_state.h"
#include "config.h"
#include "display_ui.h"
#include "metrics.h"

void themePcMonitorDrawBase()
{
  displayUiDrawDashboardBase();
}

void themePcMonitorUpdateIfNeeded()
{
  metricsUpdateDisplayIfNeeded();
  displayUiUpdateTopLabelIfNeeded();

  unsigned long now = millis();
  if (now - appState.lastClockCheck >= CLOCK_CHECK_INTERVAL_MS)
  {
    appState.lastClockCheck = now;
    displayUiUpdateHeaderIfNeeded();
  }
  if (now - appState.lastFooterUpdate >= FOOTER_UPDATE_INTERVAL_MS)
  {
    appState.lastFooterUpdate = now;
    displayUiDrawFooter();
  }
}
