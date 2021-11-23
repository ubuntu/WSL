/*
 * Copyright (C) 2021 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

namespace YAML {
	// YAML-Cpp knows how to serialize std::map, but not unordered_map.
	// This extension enables other uses of maps as well, as long as the
	// MapType provides API similar to std::map
	template <template <typename...> class MapType, typename K, typename V>
	inline Emitter& operator<<(Emitter& emitter, const MapType<K, V>& m) {
		emitter << BeginMap;
		for (const auto& v : m) {
			emitter << Key << v.first << Value << v.second;
		}

		emitter << EndMap;
		return emitter;
	}
}