/*
 * Copyright (C) 2023 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#include "FHTemplateProcessor.hpp"

#include <functional>
#include <stdexcept>
#include <iostream>

namespace FreeHeroes {

namespace {

struct DistanceRecord {
    int     m_zoneIndex  = 0;
    int64_t m_distance   = 0;
    int64_t m_zoneRadius = 0;

    int64_t dbr() const
    {
        const int64_t distanceByRadius = m_distance * 1000 / m_zoneRadius;
        return distanceByRadius;
    }

    //    int64_t r(int64_t distanceByRadius, int64_t maxRadius) const
    //    {
    //        const int64_t radius = distanceByRadius * m_zoneRadius / maxRadius;
    //        return radius;
    //    }
};

int64_t pointDistance(const FHPos& from, const FHPos& to)
{
    const auto dx = from.m_x - to.m_x;
    const auto dy = from.m_y - to.m_y;
    return static_cast<int64_t>(std::sqrt(dx * dx + dy * dy));
}

FHPos neighbour(FHPos point, int dx, int dy)
{
    point.m_x += dx;
    point.m_y += dy;
    return point;
}

struct MapDraft {
    struct Tile {
        bool m_zoned     = false;
        int  m_zoneIndex = -1;

        bool m_exFix = false;
    };

    std::map<FHPos, Tile> m_tiles;

    std::set<FHPos> m_edge;

    void init(int width,
              int height,
              int depth)
    {
        for (int z = 0; z < depth; ++z) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    m_tiles[FHPos{ x, y, z }] = {};
                }
                m_edge.insert(FHPos{ 0, y, z });
                m_edge.insert(FHPos{ width - 1, y, z });
            }
            for (int x = 0; x < width; ++x) {
                m_edge.insert(FHPos{ x, 0, z });
                m_edge.insert(FHPos{ x, height - 1, z });
            }
        }
    }

    void checkOrphans()
    {
        for (auto& [pos, cell] : m_tiles) {
            if (!cell.m_zoned)
                throw std::runtime_error("All tiles must be zoned!");
        }
    }

    bool fixExclaves() // true = nothing to fix
    {
        int fixedCount = 0;
        for (auto& [pos, cell] : m_tiles) {
            auto posT = neighbour(pos, +0, -1);
            auto posL = neighbour(pos, -1, +0);
            auto posR = neighbour(pos, +1, +0);
            auto posB = neighbour(pos, +0, +1);

            Tile& tileX = cell;

            Tile& tileT = m_tiles.contains(posT) ? m_tiles[posT] : cell;
            Tile& tileL = m_tiles.contains(posL) ? m_tiles[posL] : cell;
            Tile& tileR = m_tiles.contains(posR) ? m_tiles[posR] : cell;
            Tile& tileB = m_tiles.contains(posB) ? m_tiles[posB] : cell;

            auto processTile = [&tileX, &tileT, &tileL, &tileR, &tileB]() -> bool { // true == nothing to do
                const bool eT        = tileX.m_zoneIndex == tileT.m_zoneIndex;
                const bool eL        = tileX.m_zoneIndex == tileL.m_zoneIndex;
                const bool eR        = tileX.m_zoneIndex == tileR.m_zoneIndex;
                const bool eB        = tileX.m_zoneIndex == tileB.m_zoneIndex;
                const int  sameCount = eT + eL + eR + eB;
                if (sameCount >= 3) { // normal center / border - do nothing
                    return true;
                }
                if (sameCount == 2) {
                    if ((eT && eL) || (eT && eR) || (eB && eL) || (eB && eR))
                        return true; // corner
                    if (eT && eB) {
                        tileX.m_zoneIndex = tileL.m_zoneIndex;
                        return false;
                    }
                    if (eR && eL) {
                        tileX.m_zoneIndex = tileT.m_zoneIndex;
                        return false;
                    }
                }
                if (sameCount == 1) {
                    if (eT) {
                        tileX.m_zoneIndex = tileB.m_zoneIndex;
                    } else if (eL) {
                        tileX.m_zoneIndex = tileR.m_zoneIndex;
                    } else if (eR) {
                        tileX.m_zoneIndex = tileL.m_zoneIndex;
                    } else if (eB) {
                        tileX.m_zoneIndex = tileT.m_zoneIndex;
                    }
                    return false;
                }
                // 1 tile exclave.
                if (tileT.m_zoneIndex == tileL.m_zoneIndex) {
                    tileX.m_zoneIndex = tileT.m_zoneIndex;
                } else if (tileT.m_zoneIndex == tileR.m_zoneIndex) {
                    tileX.m_zoneIndex = tileT.m_zoneIndex;
                } else if (tileB.m_zoneIndex == tileR.m_zoneIndex) {
                    tileX.m_zoneIndex = tileB.m_zoneIndex;
                } else if (tileB.m_zoneIndex == tileL.m_zoneIndex) {
                    tileX.m_zoneIndex = tileB.m_zoneIndex;
                } else {
                    tileX.m_zoneIndex = tileT.m_zoneIndex;
                }
                return false;
            };

            /*
            auto processTileFlips = [&processTile, &tileT, &tileL, &tileR, &tileB]() -> bool {
                for (int flipHor = 0; flipHor <= 1; ++flipHor) {
                    Tile& tileLp = flipHor ? tileR : tileL;
                    Tile& tileRp = flipHor ? tileL : tileR;
                    for (int flipVert = 0; flipVert <= 1; ++flipVert) {
                        Tile& tileTp = flipVert ? tileB : tileT;
                        Tile& tileBp = flipVert ? tileT : tileB;
                        if (processTile(tileLp, tileRp, tileTp, tileBp))
                            return true;
                    }
                }
                return false;
            };*/
            if (processTile())
                continue;
            fixedCount++;
            tileX.m_exFix = true;

            //map.m_debugTiles.push_back(FHDebugTile{ .m_pos = pos, .m_valueA = cell.m_zoneIndex, .m_valueB = 0 });
        }
        return (fixedCount == 0);
    }
};

struct TileZone {
    //int m_mapWidth  = 0;
    //int m_mapHeight = 0;

    int                     m_index = 0;
    std::string             m_id;
    FHRngZone               m_rngZone;
    Core::IRandomGenerator* m_rng = nullptr;
    MapDraft*               m_map = nullptr;

    using TileRegion = std::set<FHPos>;

    FHPos      m_startTile;
    TileRegion m_innerArea;
    TileRegion m_innerEdge;   // subset of innerArea;
    TileRegion m_outsideEdge; // call grow() to make from m_innerEdge

    int64_t m_relativeArea   = 0;
    int64_t m_absoluteArea   = 0;
    int64_t m_absoluteRadius = 0;
    //int64_t    m_areaDeficit    = 0;

    int64_t getPlacedArea() const
    {
        return m_innerArea.size();
    }
    int64_t getAreaDeficit() const
    {
        return m_absoluteArea - getPlacedArea();
    }
    //TileRegion m_mapedge; // subset of edge

    void init()
    {
    }

    void readFromMap()
    {
        m_innerArea.clear();
        m_innerArea.insert(m_startTile);

        makeEdgeFromInnerArea();

        while (!m_innerEdge.empty()) {
            for (auto pos : m_innerEdge) {
                //auto& cell   = m_map->m_tiles[pos];
                //cell.m_zoned = false;
                m_innerArea.insert(pos);
            }
            // beforeGrow();
            growOnce([this](const FHPos& pos) {
                if (!m_map->m_tiles.contains(pos))
                    return false;
                if (m_innerArea.contains(pos))
                    return false;
                auto& cell = m_map->m_tiles[pos];
                return cell.m_zoned && cell.m_zoneIndex == m_index;
            });

            m_innerEdge = m_outsideEdge;
        }
        makeEdgeFromInnerArea();
    }

    void makeEdgeFromInnerArea()
    {
        m_innerEdge.clear();
        for (const FHPos& pos : m_innerArea) {
            if (m_innerArea.contains(neighbour(pos, +1, +0))
                && m_innerArea.contains(neighbour(pos, -1, +0))
                && m_innerArea.contains(neighbour(pos, +0, +1))
                && m_innerArea.contains(neighbour(pos, +0, -1)))
                continue;
            m_innerEdge.insert(pos);
        }
    }

    void writeToMap()
    {
        for (auto&& pos : m_innerArea) {
            auto& cell       = m_map->m_tiles[pos];
            cell.m_zoneIndex = m_index;
            cell.m_zoned     = true;
        }
    }

    bool isInBounds(const FHPos& point)
    {
        return m_map->m_tiles.contains(point);
    }

    void addIf(TileRegion& reg, FHPos point, auto&& predicate)
    {
        if (!predicate(point))
            return;
        reg.insert(point);
    }

    void growOnce(auto&& predicate)
    {
        m_outsideEdge.clear();
        for (auto pos : m_innerEdge) {
            addIf(m_outsideEdge, neighbour(pos, +1, +0), predicate);
            addIf(m_outsideEdge, neighbour(pos, -1, +0), predicate);
            addIf(m_outsideEdge, neighbour(pos, +0, +1), predicate);
            addIf(m_outsideEdge, neighbour(pos, +0, -1), predicate);
        }
    }

    void growOnceToUnzoned(bool allowConsumingNeighbours)
    {
        growOnce([this, allowConsumingNeighbours](const FHPos& pos) {
            if (!m_map->m_tiles.contains(pos))
                return false;
            auto& cell = m_map->m_tiles[pos];
            return !cell.m_zoned || (allowConsumingNeighbours && cell.m_zoneIndex != m_index);
        });
        m_innerEdge = m_outsideEdge;
        for (auto pos : m_innerEdge) {
            auto& cell       = m_map->m_tiles[pos];
            cell.m_zoned     = true;
            cell.m_zoneIndex = m_index;
            m_innerArea.insert(pos);
        }
    }

    void fillDeficit(int thresholdPercent, bool allowConsumingNeighbours)
    {
        const int64_t allowedDeficitThreshold = m_absoluteArea * thresholdPercent / 100;
        while (!m_innerEdge.empty()) {
            if (getAreaDeficit() < allowedDeficitThreshold)
                break;
            growOnceToUnzoned(allowConsumingNeighbours);
        }

        makeEdgeFromInnerArea();
    }

    void fillTheRest()
    {
        while (!m_innerEdge.empty()) {
            growOnceToUnzoned(false);
        }
        makeEdgeFromInnerArea(); //
    }
};
/*
class FloodFiller {
public:
    void fillAdjucent(const FHPos& current, const std::set<FHPos>& exclude, std::set<FHPos>& result, const std::function<bool(const MapTile&)>& pred)
    {
        auto addToResult = [this, &result, &exclude, &current, &pred](int dx, int dy) {
            const FHPos neighbour{ current.m_x + dx, current.m_y + dy, current.m_z };
            auto&       neighbourTile = m_srcTileSet->get(neighbour.m_x, neighbour.m_y, neighbour.m_z);
            if (pred(neighbourTile))
                return;
            if (m_zoned.contains(neighbour))
                return;
            if (exclude.contains(neighbour))
                return;
            result.insert(neighbour);
        };
        if (current.m_x < m_destTileMap->m_width - 1)
            addToResult(+1, 0);
        if (current.m_y < m_destTileMap->m_height - 1)
            addToResult(0, +1);
        if (current.m_x > 0)
            addToResult(-1, 0);
        if (current.m_y > 0)
            addToResult(0, -1);
    };

    std::vector<FHPos> makeNewZone(const FHPos& tilePos, const std::function<bool(const MapTile&)>& pred)
    {
        // now we create new zone using flood-fill;
        std::set<FHPos> newZone;
        std::set<FHPos> newZoneIter;
        newZone.insert(tilePos);
        newZoneIter.insert(tilePos);
        while (true) {
            std::set<FHPos> newFloodTiles;
            for (const FHPos& prevIterTile : newZoneIter) {
                fillAdjucent(prevIterTile, newZone, newFloodTiles, pred);
            }
            if (newFloodTiles.empty())
                break;

            newZoneIter = newFloodTiles;
            newZone.insert(newFloodTiles.cbegin(), newFloodTiles.cend());
        }

        m_zoned.insert(newZone.cbegin(), newZone.cend());
        return std::vector<FHPos>(newZone.cbegin(), newZone.cend());
    }

    FloodFiller(const MapTileSet* src, const FHTileMap* dest)
        : m_srcTileSet(src)
        , m_destTileMap(dest)
    {
    }
    const MapTileSet* const m_srcTileSet;
    const FHTileMap* const  m_destTileMap;
    std::set<FHPos>         m_zoned;
};*/

}

FHTemplateProcessor::FHTemplateProcessor(const Core::IGameDatabase* database, Core::IRandomGenerator* rng, std::ostream& logOutput)
    : m_database(database)
    , m_rng(rng)
    , m_logOutput(logOutput)
{
}

void FHTemplateProcessor::run(FHMap& map) const
{
    const int w = map.m_tileMap.m_width;
    const int h = map.m_tileMap.m_height;

    const int64_t area        = w * h;
    const int     regionCount = map.m_rngZones.size();
    if (regionCount <= 1)
        throw std::runtime_error("need at least two zones");

    MapDraft mapDraft;
    mapDraft.init(map.m_tileMap.m_width, map.m_tileMap.m_height, map.m_tileMap.m_depth);

    int64_t totalRelativeArea = 0;

    std::vector<TileZone> tileZones;
    tileZones.resize(regionCount);
    for (int i = 0; const auto& [key, rngZone] : map.m_rngZones) {
        assert(rngZone.m_terrain);
        tileZones[i].m_rngZone      = rngZone;
        tileZones[i].m_id           = key;
        tileZones[i].m_index        = i;
        tileZones[i].m_rng          = m_rng;
        tileZones[i].m_map          = &mapDraft;
        tileZones[i].m_startTile    = rngZone.m_center;
        tileZones[i].m_relativeArea = rngZone.m_relativeSize;
        totalRelativeArea += rngZone.m_relativeSize;
        if (rngZone.m_relativeSize <= 0)
            throw std::runtime_error("Zone: " + key + " has nonpositive relative size");
        i++;
    }
    if (!totalRelativeArea)
        throw std::runtime_error("Total relative area can't be zero");

    //int64_t maxRadius = 0;
    for (auto& tileZone : tileZones) {
        tileZone.m_absoluteArea   = tileZone.m_relativeArea * area / totalRelativeArea;
        tileZone.m_absoluteRadius = static_cast<int64_t>(sqrt(tileZone.m_absoluteArea) / M_PI);
        //maxRadius                 = std::max(maxRadius, tileZone.m_absoluteRadius);

        m_logOutput << "zone [" << tileZone.m_id << "] area=" << tileZone.m_absoluteArea << ", radius=" << tileZone.m_absoluteRadius << "\n";
    }
    /*
    for (auto& tileZone : tileZones) {
        tileZone.init();
        tileZone.grow(regionArea);
    }*/

    for (auto& [pos, cell] : mapDraft.m_tiles) {
        //cell.m_zoneIndex = 1;
        std::vector<DistanceRecord> distances;

        for (auto& tileZone : tileZones) {
            const auto&   zonePos  = tileZone.m_startTile;
            const int64_t distance = pointDistance(pos, zonePos);
            // const int64_t distanceByRadius = distance * maxRadius / tileZone.m_absoluteRadius;
            distances.push_back({ tileZone.m_index, distance, tileZone.m_absoluteRadius });
        }
        std::sort(distances.begin(), distances.end(), [](const DistanceRecord& l, const DistanceRecord& r) {
            return l.dbr() < r.dbr();
        });
        const DistanceRecord& first  = distances[0];
        const DistanceRecord& second = distances[1];

        const int64_t firstZoneRadius         = first.m_zoneRadius;
        const int64_t secondZoneRadius        = second.m_zoneRadius;
        const int64_t zonesTotalRadius        = firstZoneRadius + secondZoneRadius;
        const int64_t totalDistance           = first.m_distance + second.m_distance;
        const int64_t totalDistanceInRadiuses = totalDistance * 100 / zonesTotalRadius;
        const auto    distanceDiff            = totalDistanceInRadiuses * first.m_zoneRadius / 100 - first.m_distance;

        if (distanceDiff < 2)
            continue;

        cell.m_zoned     = true;
        cell.m_zoneIndex = first.m_zoneIndex;
    }

    for (auto& tileZone : tileZones) {
        tileZone.readFromMap();
        //tileZone.m_areaDeficit = tileZone.m_absoluteArea - tileZone.getPlacedArea();

        m_logOutput << "zone [" << tileZone.m_id << "] areaDeficit=" << tileZone.getAreaDeficit() << "\n";
        //tileZone.grow(regionArea);
    }
    for (auto& [pos, cell] : mapDraft.m_tiles) {
        cell.m_zoned = false;
    }
    for (auto& tileZone : tileZones) {
        tileZone.writeToMap();
    }

    std::vector<TileZone*> tileZonesPtrs(tileZones.size());
    for (size_t i = 0; i < tileZonesPtrs.size(); ++i)
        tileZonesPtrs[i] = &tileZones[i];

    auto fillDeficitIteraction = [&tileZonesPtrs, &tileZones](int thresholdPercent, bool allowConsumingNeighbours) {
        std::sort(tileZonesPtrs.begin(), tileZonesPtrs.end(), [](TileZone* l, TileZone* r) {
            return l->getAreaDeficit() > r->getAreaDeficit();
        });
        for (TileZone* zone : tileZonesPtrs) {
            zone->fillDeficit(thresholdPercent, allowConsumingNeighbours);
            if (allowConsumingNeighbours) {
                for (auto& tileZone : tileZones) {
                    tileZone.readFromMap();
                }
            }
        }
    };

    for (auto& tileZone : tileZones) {
        m_logOutput << "(before optimize) zone [" << tileZone.m_id << "] areaDeficit=" << tileZone.getAreaDeficit() << "\n";
    }

    fillDeficitIteraction(20, false);
    fillDeficitIteraction(10, true);
    fillDeficitIteraction(0, true);

    for (auto& tileZone : tileZones) {
        m_logOutput << "(after optimize) zone [" << tileZone.m_id << "] areaDeficit=" << tileZone.getAreaDeficit() << "\n";
    }

    for (TileZone* zone : tileZonesPtrs) {
        zone->fillTheRest();
    }

    mapDraft.checkOrphans();
    mapDraft.fixExclaves();

    for (int i = 0, limit = 10; i <= limit; ++i) {
        if (mapDraft.fixExclaves()) {
            m_logOutput << "exclaves fixed on [" << i << "] iteration\n";
            break;
        }
        if (i == limit) {
            throw std::runtime_error("failed to fix all exclaves after [" + std::to_string(i) + "]  iterations!");
        }
    }

    // debug exclives
    //    for (auto& [pos, cell] : mapDraft.m_tiles) {
    //        map.m_debugTiles.push_back(FHDebugTile{ .m_pos = pos, .m_valueA = cell.m_zoneIndex, .m_valueB = cell.m_exFix ? 1 : 0 });
    //    }

    //    for (auto& tileZone : tileZones) {

    //    }

    for (auto& tileZone : tileZones) {
        tileZone.readFromMap();

        FHZone fhZone;
        fhZone.m_tiles     = { tileZone.m_innerArea.cbegin(), tileZone.m_innerArea.cend() };
        fhZone.m_terrainId = tileZone.m_rngZone.m_terrain;
        assert(fhZone.m_terrainId);
        map.m_zones.push_back(std::move(fhZone));
    }

    /*
    for (auto& tileZone : tileZones) {
        map.m_debugTiles.push_back(FHDebugTile{ .m_pos = tileZone.m_startTile, .m_valueA = tileZone.m_index, .m_valueB = 1 }); // red
        for (auto& pos : tileZone.m_innerArea) {
            if (pos == tileZone.m_startTile)
                continue;
            map.m_debugTiles.push_back(FHDebugTile{ .m_pos = pos, .m_valueA = tileZone.m_index, .m_valueB = 0 });
        }
        for (auto& pos : tileZone.m_innerEdge) {
            map.m_debugTiles.push_back(FHDebugTile{ .m_pos = pos, .m_valueA = tileZone.m_index, .m_valueB = 2 }); // blue
        }
    }*/
}

}
