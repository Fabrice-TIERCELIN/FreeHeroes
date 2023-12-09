/*
 * Copyright (C) 2023 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */
#include "MapTileArea.hpp"
#include "KMeans.hpp"
#include "MapTileContainer.hpp"

#include "MernelPlatform/Profiler.hpp"

#include <iostream>
#include <unordered_set>
#include <sstream>

namespace FreeHeroes {

void MapTileArea::makeEdgeFromInnerArea()
{
    m_innerEdge = m_innerArea;
    removeNonInnerFromInnerEdge();
}

void MapTileArea::removeNonInnerFromInnerEdge()
{
    MapTilePtrSortedList forErase;

    Mernel::ProfilerScope scope("make InnerEdge");
    for (MapTilePtr cell : m_innerEdge) {
        if (m_diagonalGrowth) {
            if (m_innerArea.contains(cell->m_neighborB)
                && m_innerArea.contains(cell->m_neighborT)
                && m_innerArea.contains(cell->m_neighborR)
                && m_innerArea.contains(cell->m_neighborL)
                && m_innerArea.contains(cell->m_neighborTL)
                && m_innerArea.contains(cell->m_neighborTR)
                && m_innerArea.contains(cell->m_neighborBL)
                && m_innerArea.contains(cell->m_neighborBR))
                forErase.push_back(cell);
        } else {
            if (m_innerArea.contains(cell->m_neighborB)
                && m_innerArea.contains(cell->m_neighborT)
                && m_innerArea.contains(cell->m_neighborR)
                && m_innerArea.contains(cell->m_neighborL))
                forErase.push_back(cell);
        }
    }
    m_innerEdge.erase(forErase);

    makeOutsideEdge();
}

void MapTileArea::makeOutsideEdge()
{
    m_outsideEdge.clear();
    m_outsideEdge.reserve(m_innerEdge.size());

    for (auto* cell : m_innerEdge) {
        for (auto* ncell : cell->neighboursList(m_diagonalGrowth)) {
            if (!m_innerArea.contains(ncell))
                m_outsideEdge.insert(ncell);
        }
    }
}

void MapTileArea::removeEdgeFromInnerArea()
{
    m_innerArea.erase(m_innerEdge);
}

bool MapTileArea::refineEdge(RefineTask task, const MapTileRegion& allowedArea, size_t index)
{
    if (task == RefineTask::RemoveHollows) {
        MapTilePtrList additional;
        for (MapTilePtr cell : m_outsideEdge) {
            if (!allowedArea.contains(cell) || (cell->m_segmentIndex > 0 && cell->m_segmentIndex != index))
                continue;
            const int adjucent = m_innerArea.contains(cell->m_neighborB)
                                 + m_innerArea.contains(cell->m_neighborT)
                                 + m_innerArea.contains(cell->m_neighborR)
                                 + m_innerArea.contains(cell->m_neighborL);

            if (adjucent >= 3) {
                cell->m_segmentIndex = index;
                additional.push_back(cell);
            }
        }
        m_innerArea.insert(additional);
    }
    if (task == RefineTask::RemoveSpikes) {
        MapTilePtrList removal;
        for (MapTilePtr cell : m_innerEdge) {
            const int adjucent = m_innerArea.contains(cell->m_neighborB)
                                 + m_innerArea.contains(cell->m_neighborT)
                                 + m_innerArea.contains(cell->m_neighborR)
                                 + m_innerArea.contains(cell->m_neighborL);

            if (adjucent <= 1) {
                cell->m_segmentIndex = 0;
                removal.push_back(cell);
            }
        }
        m_innerArea.erase(removal);
    }
    if (task == RefineTask::Expand) {
        MapTilePtrList additional;
        for (MapTilePtr cell : m_outsideEdge) {
            if (!allowedArea.contains(cell) || (cell->m_segmentIndex > 0 && cell->m_segmentIndex != index))
                continue;

            cell->m_segmentIndex = index;
            additional.push_back(cell);
        }
        m_innerArea.insert(additional);
    }

    makeEdgeFromInnerArea();
    return true;
}

MapTileRegion MapTileArea::getBottomEdge() const
{
    MapTileRegion result;
    result.reserve(m_innerEdge.size() / 3);
    for (MapTilePtr cell : m_innerEdge) {
        const bool hasB      = m_innerArea.contains(cell->m_neighborB);
        const bool hasBR     = m_innerArea.contains(cell->m_neighborBR);
        const bool hasBL     = m_innerArea.contains(cell->m_neighborBL);
        const int  hasBCount = hasB + hasBR + hasBL;
        if (hasBCount < 2)
            result.insert(cell);
    }
    return result;
}

MapTileArea MapTileArea::floodFillDiagonalByInnerEdge(MapTilePtr cellStart) const
{
    MapTileArea innerEdgeArea;
    innerEdgeArea.m_innerArea = this->m_innerEdge;

    auto segments = innerEdgeArea.splitByFloodFill(true, cellStart);
    if (segments.empty())
        return {};
    return segments[0];
}

std::vector<MapTileArea> MapTileArea::splitByFloodFill(bool useDiag, MapTilePtr hint) const
{
    if (m_innerArea.empty())
        return {};

    std::vector<MapTileArea> result;
    MapTilePtrList           currentBuffer;

    MapTileRegion  visited;
    MapTilePtrList currentEdge;
    auto           addToCurrent = [&currentBuffer, &visited, &currentEdge, *this](MapTilePtr cell) {
        if (visited.contains(cell))
            return;
        if (!m_innerArea.contains(cell))
            return;
        visited.insert(cell);
        currentBuffer.push_back(cell);
        currentEdge.push_back(cell);
    };
    MapTileRegion remain = m_innerArea;
    if (hint) {
        if (!remain.contains(hint))
            throw std::runtime_error("Invalid tile hint provided");
    }

    while (!remain.empty()) {
        MapTilePtr startCell = hint ? hint : *remain.begin();
        hint                 = nullptr;
        addToCurrent(startCell);

        while (!currentEdge.empty()) {
            MapTilePtrList nextEdge = std::move(currentEdge);

            for (MapTilePtr edgeCell : nextEdge) {
                const auto& neighbours = edgeCell->neighboursList(useDiag);
                for (auto growedCell : neighbours) {
                    addToCurrent(growedCell);
                }
            }
        }
        MapTileArea current;
        current.m_innerArea = MapTileRegion(std::move(currentBuffer));
        current.makeEdgeFromInnerArea();
        remain.erase(current.m_innerArea);
        result.push_back(std::move(current));
    }

    return result;
}

std::vector<MapTileArea> MapTileArea::splitByMaxArea(std::ostream& os, size_t maxArea, bool repulse) const
{
    std::vector<MapTileArea> result;
    size_t                   zoneArea = m_innerArea.size();
    if (!zoneArea)
        return result;

    const size_t k = (zoneArea + maxArea + 1) / maxArea;

    return splitByK(os, k, repulse);
}

std::vector<MapTileArea> MapTileArea::splitByK(std::ostream& os, size_t k, bool repulse) const
{
    std::vector<MapTileArea> result;
    size_t                   zoneArea = m_innerArea.size();
    if (!zoneArea)
        return result;

    if (k == 1) {
        result.push_back(*this);
    } else {
        KMeansSegmentation seg;
        seg.m_points.reserve(zoneArea);
        for (auto* cell : m_innerArea) {
            seg.m_points.push_back({ cell });
        }
        seg.m_iters = 30;
        seg.initEqualCentoids(k);
        seg.run(os);

        std::vector<KMeansSegmentation::Cluster*> clusters;
        for (KMeansSegmentation::Cluster& cluster : seg.m_clusters)
            clusters.push_back(&cluster);
        if (repulse) {
            std::vector<KMeansSegmentation::Cluster*> clustersSorted;
            auto*                                     curr = clusters.back();
            clusters.pop_back();
            clustersSorted.push_back(curr);
            while (!clusters.empty()) {
                auto it = std::max_element(clusters.begin(), clusters.end(), [curr](KMeansSegmentation::Cluster* l, KMeansSegmentation::Cluster* r) {
                    return posDistance(curr->m_centroid, l->m_centroid) < posDistance(curr->m_centroid, r->m_centroid);
                });
                curr    = *it;
                clustersSorted.push_back(curr);
                clusters.erase(it);
            }
            clusters = clustersSorted;
        }

        for (KMeansSegmentation::Cluster* cluster : clusters) {
            MapTileRegion zoneSeg;
            zoneSeg.reserve(cluster->m_points.size());
            for (auto& point : cluster->m_points)
                zoneSeg.insert(point->m_pos);

            assert(zoneSeg.size() > 0);
            result.push_back(MapTileArea{ .m_innerArea = std::move(zoneSeg) });
        }
    }
    for (auto& area : result)
        area.makeEdgeFromInnerArea();
    return result;
}

MapTilePtr MapTileArea::makeCentroid(const MapTileRegion& region, bool ensureInbounds)
{
    if (region.empty())
        return nullptr;

    MapTileContainer* tileContainer = region[0]->m_container;

    int64_t   sumX = 0, sumY = 0;
    int64_t   size = region.size();
    const int z    = region[0]->m_pos.m_z;
    for (const auto* cell : region) {
        sumX += cell->m_pos.m_x;
        sumY += cell->m_pos.m_y;
    }
    sumX /= size;
    sumY /= size;
    const auto pos      = FHPos{ static_cast<int>(sumX), static_cast<int>(sumY), z };
    MapTilePtr centroid = tileContainer->m_tileIndex.at(pos);
    if (ensureInbounds && !region.contains(centroid)) {
        auto it  = std::min_element(region.cbegin(), region.cend(), [centroid](MapTilePtr l, MapTilePtr r) {
            return posDistance(centroid, l, 100) < posDistance(centroid, r, 100);
        });
        centroid = (*it);
    }

    /// let's make centroid tile as close to center of mass as possible

    auto estimateAvgDistance = [&region](MapTilePtr centerTile) -> int64_t {
        int64_t result = 0;
        for (const auto* cell : region) {
            result += posDistance(centerTile, cell, 100);
        }
        return result;
    };
    int64_t result = estimateAvgDistance(centroid);

    for (MapTilePtr tile : centroid->m_allNeighboursWithDiag) {
        if (ensureInbounds && !region.contains(tile))
            continue;
        auto altResult = estimateAvgDistance(tile);
        if (altResult < result) {
            result   = altResult;
            centroid = tile;
        }
    }

    return centroid;
}

MapTileArea MapTileArea::getInnerBorderNet(const std::vector<MapTileArea>& areas)
{
    MapTileArea result;
    for (size_t i = 0; i < areas.size(); ++i) {
        const MapTileArea& areaX = areas[i];
        for (size_t k = i + 1; k < areas.size(); ++k) {
            const MapTileArea& areaY = areas[k];
            for (auto* innerCellX : areaX.m_innerEdge) {
                if (areaY.m_outsideEdge.contains(innerCellX))
                    result.m_innerArea.insert(innerCellX);
            }
        }
    }
    return result;
}

std::pair<MapTileArea::CollisionResult, FHPos> MapTileArea::getCollisionShiftForObject(const MapTileRegion& object, const MapTileRegion& obstacle, bool invertObstacle)
{
    if (object.empty() || obstacle.empty())
        return std::pair{ CollisionResult::InvalidInputs, FHPos{} };

    const MapTileRegion intersection = invertObstacle ? object.diffWith(obstacle) : object.intersectWith(obstacle);
    if (intersection.empty())
        return std::pair{ CollisionResult::NoCollision, FHPos{} };

    if (intersection == object)
        return std::pair{ CollisionResult::ImpossibleShift, FHPos{} };

    const MapTilePtr collisionCentroid = MapTileArea::makeCentroid(intersection, false);

    MapTileRegion objectWithoutObstacle = object;
    objectWithoutObstacle.erase(collisionCentroid);

    FHPos topLeftBoundary = object[0]->m_pos, rightBottomBoundary = object[0]->m_pos;
    for (auto* tile : object) {
        topLeftBoundary.m_x     = std::min(topLeftBoundary.m_x, tile->m_pos.m_x);
        topLeftBoundary.m_y     = std::min(topLeftBoundary.m_y, tile->m_pos.m_y);
        rightBottomBoundary.m_x = std::max(rightBottomBoundary.m_x, tile->m_pos.m_x);
        rightBottomBoundary.m_y = std::max(rightBottomBoundary.m_y, tile->m_pos.m_y);
    }
    FHPos     diff       = rightBottomBoundary - topLeftBoundary;
    const int width      = diff.m_x + 1;
    const int height     = diff.m_y + 1;
    const int horRadius  = width / 2; // 1x1 => 0, 2x2 -> 1, 3x3 -> 1 , 4x4 -> 2
    const int vertRadius = height / 2;

    const MapTilePtr objectCentroid = MapTileArea::makeCentroid(objectWithoutObstacle, false);

    FHPos collisionOffset = objectCentroid->m_pos - collisionCentroid->m_pos;
    int   cx              = collisionOffset.m_x;
    int   cy              = collisionOffset.m_y;
    if (cx == 0 && cy == 0)
        return std::pair{ CollisionResult::ImpossibleShift, FHPos{} };

    if (cx > 0) {
        if (horRadius > 1)
            cx = horRadius - cx + 1;
    }
    if (cx < 0) {
        if (horRadius > 1)
            cx = -horRadius - cx - 1;
    }
    if (cy > 0) {
        if (vertRadius > 1)
            cy = vertRadius - cy + 1;
    }
    if (cy < 0) {
        if (horRadius > 1)
            cy = -vertRadius - cy - 1;
    }
    return std::pair{ CollisionResult::HasShift, FHPos{ cx, cy } };
}

void MapTileArea::decompose(MapTileContainer* tileContainer, MapTileRegion& object, MapTileRegion& obstacle, const std::string& serialized, int width, int height)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto*        tile             = tileContainer->m_tileIndex.at(FHPos{ x, y, 0 });
            const size_t testOffset       = x + width * y;
            const char   c                = serialized[testOffset];
            const bool   objectOccupied   = c == 'O' || c == 'X';
            const bool   obstacleOccupied = c == '-' || c == 'X';
            if (objectOccupied)
                object.insert(tile);
            if (obstacleOccupied)
                obstacle.insert(tile);
        }
    }
}

void MapTileArea::compose(const MapTileRegion& object, const MapTileRegion& obstacle, std::string& serialized, bool obstacleInverted, bool printable)
{
    MapTileContainer* tileContainer = nullptr;
    if (!object.empty())
        tileContainer = object[0]->m_container;
    if (!obstacle.empty())
        tileContainer = obstacle[0]->m_container;
    if (!tileContainer)
        return;

    const int z      = object.empty() ? obstacle[0]->m_pos.m_z : object[0]->m_pos.m_z;
    const int width  = tileContainer->m_width;
    const int height = tileContainer->m_height;
    serialized.clear();
    for (int y = 0; y < height; ++y) {
        if (printable)
            serialized += '"';
        for (int x = 0; x < width; ++x) {
            auto* tile = tileContainer->m_tileIndex.at(FHPos{ x, y, z });
            //const size_t testOffset = x + width * y;
            char c;

            const bool objectOccupied   = object.contains(tile);
            const bool obstacleOccupied = obstacleInverted ? !obstacle.contains(tile) : obstacle.contains(tile);
            if (objectOccupied && obstacleOccupied)
                c = 'X';
            else if (objectOccupied)
                c = 'O';
            else if (obstacleOccupied)
                c = '-';
            else
                c = '.';
            serialized += c;
        }
        if (printable)
            serialized += '"', serialized += '\n';
    }
}

}
