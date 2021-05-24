#ifndef OPENVRDOMAIN_H
#define OPENVRDOMAIN_H

#include <functional>
#include <iostream>

#include "al/app/al_App.hpp"
#include "al/app/al_OpenGLGraphicsDomain.hpp"
#include "al_ext/openvr/al_OpenVRWrapper.hpp"

namespace al {

class OpenVRDomain : public SynchronousDomain {
 public:
  virtual ~OpenVRDomain() {}

  // Domain management functions
//  bool initialize(ComputationDomain *parent = nullptr) override;
bool initialize(ComputationDomain *parent = nullptr) ;
    bool tick() override;
  bool cleanup(ComputationDomain *parent = nullptr) override;

  void setDrawFunction(std::function<void(Graphics &)> func) {
    drawSceneFunc = func;
  }

  static std::shared_ptr<OpenVRDomain> enableVR(App *app) {
#ifdef AL_EXT_OPENVR
    auto vrDomain = app->graphicsDomain()->newSubDomain<OpenVRDomain>(true);
    vrDomain->initialize(app->graphicsDomain().get());
    return vrDomain;
#else
    (void)app;
    std::cout << "OpenVR support not available. Ignoring enableVR()"
              << std::endl;
    return nullptr;
#endif
  }

  static void disableVR(App *app, std::shared_ptr<OpenVRDomain> openVRDomain) {
#ifdef AL_EXT_OPENVR
    assert(openVRDomain);
    app->graphicsDomain()->removeSubDomain(openVRDomain);
#else
    (void)openVRDomain;
    std::cout << "OpenVR support not available. Ignoring enableVR()"
              << std::endl;
#endif
  }

  al::OpenVRWrapper mOpenVR;
  std::unique_ptr<Graphics> mGraphics;

 private:
  std::function<void(Graphics &)> drawSceneFunc = [](Graphics &g) {
    g.clear(1.0, 0, 0.0);
  };
};

}  // namespace al

#endif  // OPENVRDOMAIN_H
