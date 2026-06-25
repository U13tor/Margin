// HostServicesImpl — see host/services/HostServicesImpl.h.

#include "HostServicesImpl.h"

namespace Margin {

HostServicesImpl::HostServicesImpl(Logger& logger,
                                   EventBus& eventBus,
                                   Settings& settings,
                                   TrayService& tray,
                                   CryptoService& crypto)
    : m_logger(logger)
    , m_eventBus(eventBus)
    , m_settings(settings)
    , m_tray(tray)
    , m_crypto(crypto) {}

} // namespace Margin
