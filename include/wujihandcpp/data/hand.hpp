#pragma once

#include <cstdint>

#include "wujihandcpp/data/helper.hpp"

namespace wujihandcpp {

namespace device {
class Hand;
class Finger;
class Joint;
}; // namespace device

namespace data {
namespace hand {
struct FirmwareVersion : ReadOnlyData<device::Hand, 0x5201, 1, uint32_t> {};
struct FirmwareDate : ReadOnlyData<device::Hand, 0x5201, 2, uint32_t> {};

struct SystemTime : ReadOnlyData<device::Hand, 0x520A, 1, uint32_t> {};
struct McuTemperature : ReadOnlyData<device::Hand, 0x520A, 9, float> {};
struct InputVoltage : ReadOnlyData<device::Hand, 0x520A, 10, float> {};

struct PdoEnabled : WriteOnlyData<device::Hand, 0x52A0, 5, uint8_t> {};

struct GlobalTpdoId : WriteOnlyData<device::Hand, 0x52A4, 2, uint16_t> {};
struct JointPdoInterval : WriteOnlyData<device::Hand, 0x52A4, 5, uint32_t> {};
} // namespace hand
} // namespace data

} // namespace wujihandcpp