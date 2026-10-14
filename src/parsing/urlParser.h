#pragma once

#include <optional>
#include <string>

std::optional<std::string> resolveUrl(
	const std::string& baseUrl,
	const std::string& link
);