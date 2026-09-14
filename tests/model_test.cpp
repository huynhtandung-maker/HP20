#include "../model.h"
#include <cassert>
#include <iostream>
int main() {
  // NWS chart: 90 F / 70% RH -> approximately 106 F.
  assert(std::abs(model::heatIndex(32.222222,70)-41.1)<0.2);
  assert(std::isnan(model::heatIndex(25,70)));
  assert(std::isnan(model::heatIndex(30,101)));
  assert(std::isnan(model::heatIndex(NAN,70)));
  assert(model::heatIndex(32,80)>model::heatIndex(32,60));
  assert(!model::validSample(NAN,50));
  assert(!model::validSample(25,-1));
  using B=model::RoomBand;
  assert(model::roomBand(26,65,NAN)==B::Comfortable);
  assert(model::roomBand(28,76,NAN)==B::Humid);
  assert(model::roomBand(30,65,34)==B::Warm);
  assert(model::roomBand(32,70,40)==B::Hot);
  assert(model::roomBand(35.9,61,48.3)==B::SevereHeat);
  assert(model::roomBand(NAN,61,48)==B::Invalid);
  assert(model::elapsed(50,UINT32_MAX-49,100));
  assert(!model::elapsed(49,UINT32_MAX-49,100));
  model::Reminder r;
  assert(!r.update(0,36,true,35));
  assert(!r.update(119999,36,true,35));
  assert(r.update(120000,36,true,35));
  assert(r.update(120001,34.5,true,35));
  assert(!r.update(120002,33.9,true,35));
  assert(!r.update(130000,36,false,35));
  assert(!r.update(140000,36,true,35));
  assert(!r.update(150000,NAN,true,35));
  assert(!r.update(270000,36,true,35));
  assert(model::retrySeconds(900,1)==1800);
  assert(model::retrySeconds(900,10)==21600);
  assert(model::retrySeconds(86400,10)==86400);
  using E=model::ButtonEvent;
  model::Button button;
  assert(button.update(0,true)==E::None);
  assert(button.update(10,false)==E::None); // Contact bounce: no click.
  assert(button.update(60,false)==E::None);
  assert(button.update(100,true)==E::None);
  assert(button.update(140,true)==E::None);
  assert(button.update(200,false)==E::None);
  assert(button.update(240,false)==E::Click);
  assert(button.update(280,false)==E::None); // Exactly one event.
  assert(button.update(300,true)==E::None);
  assert(button.update(340,true)==E::None);
  assert(button.update(3339,true)==E::None);
  assert(button.update(3340,true)==E::Hold);
  assert(button.update(4000,true)==E::None);
  assert(button.update(4010,false)==E::None);
  assert(button.update(4050,false)==E::None); // Hold release must not click.
  model::Button wrapped;
  assert(wrapped.update(UINT32_MAX-99,true)==E::None);
  assert(wrapped.update(UINT32_MAX-59,true)==E::None);
  assert(wrapped.update(2940,true)==E::Hold);
  const uint64_t epoch=1800000000;
  assert(model::remainingSeconds(0,0,900,epoch,0)==900); // Reboot waits full interval.
  assert(model::remainingSeconds(899999,0,900,epoch,0)==1);
  assert(model::remainingSeconds(900000,0,900,epoch,0)==0);
  assert(model::remainingSeconds(900000,0,900,epoch,epoch+86400)==86400); // HTTP 429 survives boot/config save.
  assert(model::remainingSeconds(0,0,900,epoch,epoch+30)==900);
  assert(model::remainingSeconds(1000,0,900,0,epoch)==899); // Unsynced clock handled by cloud guard.
  assert(model::remainingSeconds(500,UINT32_MAX-499,900,epoch,0)==899);
  // 31 days, one-second simulation, including hourly reboot before a send slot.
  uint32_t mark=0, uptime=0, sent=0;
  uint64_t deadline=0;
  for (uint32_t second=0;second<31UL*86400;second++) {
    if (second && second%3600==0) { uptime=0; mark=0; }
    if (model::remainingSeconds(uptime,mark,900,epoch+second,deadline)==0) {
      assert(epoch+second>=deadline);
      deadline=epoch+second+900; mark=uptime; sent++;
    }
    uptime+=1000;
  }
  assert(sent>0 && sent<=2976);
  // Restarting every ten minutes cannot bypass the full boot delay.
  for (uint32_t second=0;second<86400;second++)
    assert(model::remainingSeconds((second%600)*1000,0,900,epoch+second,0)>0);
  // A backward wall-clock adjustment keeps the persisted reservation in force.
  assert(model::remainingSeconds(900000,0,900,epoch-3600,epoch+900)==4500);
  std::cout << "Model, button, cooldown and 31-day reboot simulation passed\n";
}
