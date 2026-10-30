#pragma once

#include <initializer_list>
#include <utility>
#include <set>
#include <string>
#include <string_view>

namespace simplecam {

// Decode only our generated wrapper, once. OBS may append a legacy suffix
// after </span>; keep it as part of the normalized name.
inline std::string plainPresetSourceName(std::string name)
{
	const std::string opening = "<span style=\"color:#FF6050;\">";
	const auto end = name.find("</span>", opening.size());
	if (name.compare(0, opening.size(), opening) != 0 || end == std::string::npos)
		return name;
	std::string text = name.substr(opening.size(), end - opening.size());
	if (text.size() >= 2 && text.compare(text.size() - 2, 2, " *") == 0)
		text.resize(text.size() - 2);
	std::string decoded;
	for (size_t i = 0; i < text.size();) {
		bool matched = false;
		for (const auto &entity : {std::pair<const char *, char>{"&lt;", '<'}, {"&gt;", '>'},
					 {"&quot;", '"'}, {"&amp;", '&'}}) {
			const std::string_view token(entity.first);
			if (text.compare(i, token.size(), token) == 0) {
				decoded += entity.second;
				i += token.size();
				matched = true;
				break;
			}
		}
		if (!matched) decoded += text[i++];
	}
	return decoded + name.substr(end + 7);
}

// -1 means unrelated/malformed. Zero represents the unsuffixed identity.
inline int presetSourceNumber(const std::string &base, const std::string &rawName)
{
	const std::string name = plainPresetSourceName(rawName);
	if (name == base) return 0;
	const std::string prefix = base + " (";
	if (name.compare(0, prefix.size(), prefix) != 0 || name.back() != ')') return -1;
	const size_t end = name.size() - 1;
	if (prefix.size() == end) return -1;
	int number = 0;
	for (size_t i = prefix.size(); i < end; ++i) {
		if (name[i] < '0' || name[i] > '9') return -1;
		number = number * 10 + name[i] - '0';
		if (number > 99999) return -1;
	}
	return number >= 1 ? number : -1;
}

class PresetNameReservations {
public:
	explicit PresetNameReservations(std::string base) : base_(std::move(base)) {}
	void reserveName(const std::string &name)
	{
		const int number = presetSourceNumber(base_, name);
		if (number >= 0) numbers_.insert(number);
	}
	void reserveIdentity(const std::string &base, int number)
	{
		if (base == base_ && number >= 0 && number <= 99999) numbers_.insert(number);
	}
	bool occupied(int number) const { return numbers_.count(number) != 0; }
	int lowestUnused(bool requireSuffix) const
	{
		if (!requireSuffix && !occupied(0)) return 0;
		for (int number = 2; number <= 99999; ++number)
			if (!occupied(number)) return number;
		return -1; // Exhaustion: caller must leave the source/reference unchanged.
	}
private:
	std::string base_;
	std::set<int> numbers_;
};
} // namespace simplecam
