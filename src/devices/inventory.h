#pragma once
#include "catalog.h"
#include <QVariantList>

namespace open_icue {
/** Read USB ancestry from libudev, never opening device command endpoints. */
QVariantList discoverUsb(const Catalog& catalog);
} // namespace open_icue
