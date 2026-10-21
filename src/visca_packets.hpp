#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace simplecam {

using ViscaPacket = std::vector<std::uint8_t>;

inline std::uint8_t addressByte(int cameraAddress)
{
	if (cameraAddress < 1 || cameraAddress > 7)
		throw std::out_of_range("VISCA camera address must be 1 through 7");
	return static_cast<std::uint8_t>(0x80 + cameraAddress);
}

inline ViscaPacket makePanTilt(int cameraAddress, int panSpeed, int tiltSpeed, std::uint8_t panDirection,
			       std::uint8_t tiltDirection)
{
	if (panSpeed < 1 || panSpeed > 24 || tiltSpeed < 1 || tiltSpeed > 20)
		throw std::out_of_range("VISCA pan/tilt speed is outside the AViPAS UI range");
	if (panDirection < 1 || panDirection > 3 || tiltDirection < 1 || tiltDirection > 3)
		throw std::out_of_range("VISCA direction must be in the range 1 through 3");

	return {addressByte(cameraAddress), 0x01, 0x06, 0x01, static_cast<std::uint8_t>(panSpeed),
		static_cast<std::uint8_t>(tiltSpeed), panDirection, tiltDirection, 0xFF};
}

inline ViscaPacket makeStopPanTilt(int cameraAddress, int panSpeed, int tiltSpeed)
{
	return makePanTilt(cameraAddress, panSpeed, tiltSpeed, 0x03, 0x03);
}

inline ViscaPacket makeZoomDrive(int cameraAddress, bool tele)
{
	return {addressByte(cameraAddress), 0x01, 0x04, 0x07, static_cast<std::uint8_t>(tele ? 0x20 : 0x30), 0xFF};
}

inline ViscaPacket makeZoomStop(int cameraAddress)
{
	return {addressByte(cameraAddress), 0x01, 0x04, 0x07, 0x00, 0xFF};
}

inline ViscaPacket makeHome(int cameraAddress)
{
	return {addressByte(cameraAddress), 0x01, 0x06, 0x04, 0xFF};
}

inline ViscaPacket makePresetRecall(int cameraAddress, int preset)
{
	if (preset < 1 || preset > 255)
		throw std::out_of_range("VISCA preset must be in the range 1 through 255");
	return {addressByte(cameraAddress), 0x01, 0x04, 0x3F, 0x02, static_cast<std::uint8_t>(preset), 0xFF};
}

inline std::vector<ViscaPacket> makePresetStoreSequence(int cameraAddress, int preset)
{
	if (preset < 1 || preset > 255)
		throw std::out_of_range("VISCA preset must be in the range 1 through 255");
	const auto address = addressByte(cameraAddress);
	const auto number = static_cast<std::uint8_t>(preset);
	return {{address, 0x01, 0x04, 0x3F, 0x00, number, 0xFF},
		{address, 0x01, 0x04, 0x3F, 0x01, number, 0xFF},
		{address, 0x01, 0x7E, 0x01, 0x0B, number, 0x10, 0xFF}};
}

inline ViscaPacket makeBacklight(int cameraAddress, bool enabled)
{
	return {addressByte(cameraAddress), 0x01, 0x04, 0x33, static_cast<std::uint8_t>(enabled ? 0x02 : 0x03), 0xFF};
}

} // namespace simplecam
