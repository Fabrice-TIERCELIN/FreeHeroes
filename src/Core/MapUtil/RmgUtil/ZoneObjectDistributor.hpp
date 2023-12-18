/*
 * Copyright (C) 2023 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#pragma once

#include "ZoneObject.hpp"

#include "MapTileRegionWithEdge.hpp"

namespace FreeHeroes {

namespace Core {
class IGameDatabase;
class IRandomGenerator;
}

struct TileZone;
struct FHMap;
class MapTileContainer;
struct ZoneObjectWrap : public ZoneObjectItem {
    enum class GuardPosition
    {
        TL,
        T,
        TR,
        L,
        R,
        BL,
        B,
        BR,
    };

    MapTilePtr m_absPos        = nullptr;
    MapTilePtr m_guardAbsPos   = nullptr;
    FHPos      m_centerOffset  = g_invalidPos;
    bool       m_considerBlock = false;

    MapTileRegion m_rewardArea{};
    MapTileRegion m_extraObstacles{};

    MapTileRegion m_unpassableArea{}; // extraObstacles + [rewardArea optional]
    MapTileRegion m_occupiedArea{};   // tiles that physically takes place on map (both removable and permanent). reward+obstacles+guard

    MapTileRegion m_dangerZone{};             // tiles that under attack of guard but not occupied
    MapTileRegion m_occupiedWithDangerZone{}; // occupied + danger

    MapTileRegion m_passAroundEdge{};
    MapTileRegion m_allArea{}; // occupied + danger + passAround

    //MapTileRegion m_lastCellSource;

    bool m_absPosIsValid = false;

    int    m_placedHeat           = 0;
    size_t m_segmentIndex         = 0;
    size_t m_segmentFragmentIndex = 0;
    size_t m_estimatedArea        = 0;

    FHPos m_radiusVectorAbsPos = g_invalidPos;

    bool estimateOccupied(MapTilePtr absPosCenter);

    std::string toPrintableString() const;

    void place() const;
};
using ZoneObjectWrapList    = std::vector<ZoneObjectWrap>;
using ZoneObjectWrapPtrList = std::vector<ZoneObjectWrap*>;

class ZoneObjectDistributor {
public:
    struct DistributionResult;
    struct ZoneSegment {
        ZoneObjectWrapPtrList m_successNormal;

        MapTileRegion m_originalArea;
        MapTileRegion m_freeArea;

        MapTilePtr m_originalAreaCentroid = nullptr;

        struct HeatDataItem {
            MapTilePtr    m_centroid = nullptr;
            MapTileRegion m_free;
            int           m_heat = 0;
        };

        std::map<int, HeatDataItem> m_heatMap;
        std::map<MapTilePtr, int>   m_distances;

        const TileZone* m_tileZone = nullptr;

        size_t m_segmentIndex = 0;

        int getFreePercent() const { return static_cast<int>(m_freeArea.size() * 100 / m_originalArea.size()); }

        std::string toPrintableString() const;

        HeatDataItem* findBestHeatData(int heat);

        void compactIfNeeded();
        void commitPlacement(DistributionResult& distribution, ZoneObjectWrap* object);
        void recalcFree(ZoneObjectWrap* exclude = nullptr);
        void recalcHeat();
    };
    using ZoneSegmentList = std::vector<ZoneSegment>;

    struct Guard {
        int64_t    m_value = 0;
        MapTilePtr m_pos   = nullptr;
    };
    using GuardList = std::vector<Guard>;

    struct DistributionResult {
        int m_maxHeat = 0;

        ZoneObjectWrapList m_allObjects;

        ZoneSegmentList m_segments;
        GuardList       m_guards;
        MapTileRegion   m_needBlock;

        std::vector<std::string> m_allOriginalIds; // for checking
        std::vector<std::string> m_placedIds;      // for checking

        ZoneObjectWrapPtrList m_candidateObjectsFreePickables;
        ZoneObjectWrapPtrList m_roadPickables;

        const TileZone* m_tileZone = nullptr;

        void init(TileZone& tileZone);
    };

    ZoneObjectDistributor(FHMap&                        map,
                          Core::IRandomGenerator* const rng,
                          MapTileContainer&             tileContainer,
                          std::ostream&                 logOutput)
        : m_map(map)
        , m_rng(rng)
        , m_tileContainer(tileContainer)
        , m_logOutput(logOutput)
    {
    }

    bool makeInitialDistribution(DistributionResult& distribution, const ZoneObjectGeneration& generated) const;
    void doPlaceDistribution(DistributionResult& distribution) const;

private:
    bool placeWrapIntoSegments(DistributionResult& distribution, ZoneObjectWrap* object, std::vector<ZoneSegment*>& segCandidates) const;
    void commitPlacement(DistributionResult& distribution, ZoneObjectWrap* object, ZoneSegment* seg) const;

private:
    const std::string       m_indent = "         ";
    FHMap&                  m_map;
    Core::IRandomGenerator* m_rng = nullptr;
    MapTileContainer&       m_tileContainer;
    std::ostream&           m_logOutput;
};

}
