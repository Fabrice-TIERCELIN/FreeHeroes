/*
 * Copyright (C) 2023 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#pragma once

#include "MapTile.hpp"
#include "MapTileRegion.hpp"

#include "MapUtilExport.hpp"

#include <set>
#include <unordered_map>

namespace FreeHeroes {

class MAPUTIL_EXPORT MapTileContainer {
public:
    int m_width  = 0;
    int m_height = 0;
    int m_depth  = 0;

    MapTileRegion m_all;

    std::unordered_map<FHPos, MapTilePtr> m_tileIndex;
    MapTilePtr                            m_centerTile = nullptr;

    std::set<TileZone*> m_dirtyZones; // zone ids that must be re-read from the map.

    void init(int width, int height, int depth);

    bool fixExclaves();

private:
    std::vector<MapTile> m_tiles;
};
}
