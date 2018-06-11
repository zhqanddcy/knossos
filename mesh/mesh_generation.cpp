#include "mesh_generation.h"

#include "coordinate.h"
#include "dataset.h"
#include "loader.h"
#include "segmentation/cubeloader.h"
#include "segmentation/segmentation.h"
#include "skeleton/skeletonizer.h"

#include "vtkMarchingCubesTriangleCases.h"

#include <QApplication>

#include <snappy.h>

#include <unordered_map>

void generateMeshForFirstSubobjectOfFirstSelectedObject() {
    const auto value = Segmentation::singleton().objects[Segmentation::singleton().selectedObjectIndices.front()].subobjects.front().get().id;
    std::unordered_map<floatCoordinate, int> points;
    QVector<unsigned int> faces;
    std::size_t idCounter{0};

    const auto & cubes = Loader::Controller::singleton().getAllModifiedCubes();
    for (const auto & pair : cubes[0]) {
        const auto cubeEdgeLen = 128;
        const auto cubeCoord = Dataset::current().scale.componentMul(pair.first.cube2Global(cubeEdgeLen, 1));
        const std::size_t size = std::pow(cubeEdgeLen, 3);
        const auto triCases = vtkMarchingCubesTriangleCases::GetCases();

        const std::array<double, 3> dims{{cubeEdgeLen, cubeEdgeLen, cubeEdgeLen}};
        const std::array<double, 3> origin{{cubeCoord.x, cubeCoord.y, cubeCoord.z}};
        const std::array<double, 3> spacing{{Dataset::current().scale.x, Dataset::current().scale.y, Dataset::current().scale.z}};
        const std::array<double, 6> extent{{0, dims[0], 0, dims[1], 0, dims[2]}};

        static int CASE_MASK[8]{1,2,4,8,16,32,64,128};
        static int edges[12][2]{{0,1}, {1,2}, {3,2}, {0,3}, {4,5}, {5,6}, {7,6}, {4,7}, {0,4}, {1,5}, {3,7}, {2,6}};

        std::array<std::uint64_t, 8> cubeVals;

        std::vector<std::uint64_t> data(size);
        if (!snappy::RawUncompress(pair.second.c_str(), pair.second.size(), reinterpret_cast<char *>(data.data()))) {
            continue;
        }

        const auto rowSize = dims[0];
        const auto sliceSize = rowSize * dims[1];

        for (std::size_t z = 0; z < (dims[2] - 1); ++z) {
            const auto zOffset = z * sliceSize;
            std::array<std::array<double, 3>, 8> pts;
            pts[0][2] = origin[2] + (z + extent[4]) * spacing[2];
            const auto znext = pts[0][2] + spacing[2];
            for (std::size_t y = 0; y < (dims[1] - 1); ++y) {
                const auto yOffset = y * rowSize;
                pts[0][1] = origin[1] + (y + extent[2]) * spacing[1];
                const auto ynext = pts[0][1] + spacing[1];
                for (std::size_t x = 0; x < (dims[0] - 1); ++x) {
                    // get scalar values
                    const auto idx = x + yOffset + zOffset;
                    cubeVals[0] = data[idx];
                    cubeVals[1] = data[idx+1];
                    cubeVals[2] = data[idx+1 + dims[0]];
                    cubeVals[3] = data[idx + dims[0]];
                    cubeVals[4] = data[idx + sliceSize];
                    cubeVals[5] = data[idx+1 + sliceSize];
                    cubeVals[6] = data[idx+1 + dims[0] + sliceSize];
                    cubeVals[7] = data[idx + dims[0] + sliceSize];

                    // create voxel points
                    pts[0][0] = origin[0] + (x + extent[0]) * spacing[0];
                    const auto xnext = pts[0][0] + spacing[0];

                    pts[1][0] = xnext;
                    pts[1][1] = pts[0][1];
                    pts[1][2] = pts[0][2];

                    pts[2][0] = xnext;
                    pts[2][1] = ynext;
                    pts[2][2] = pts[0][2];

                    pts[3][0] = pts[0][0];
                    pts[3][1] = ynext;
                    pts[3][2] = pts[0][2];

                    pts[4][0] = pts[0][0];
                    pts[4][1] = pts[0][1];
                    pts[4][2] = znext;

                    pts[5][0] = xnext;
                    pts[5][1] = pts[0][1];
                    pts[5][2] = znext;

                    pts[6][0] = xnext;
                    pts[6][1] = ynext;
                    pts[6][2] = znext;

                    pts[7][0] = pts[0][0];
                    pts[7][1] = ynext;
                    pts[7][2] = znext;

                    std::size_t index = 0;
                    // Build the case table
                    for (std::size_t pi = 0; pi < 8; ++pi) {
                        // for discrete marching cubes, we are looking for an
                        // exact match of a scalar at a vertex to a value
                        if (cubeVals[pi] == value) {
                            index |= CASE_MASK[pi];
                        }
                    }
                    if (index == 0 || index == 255) {// no surface
                        continue;
                    }

                    const auto triCase = triCases + index;
                    for (auto edge = triCase->edges; edge[0] > -1; edge += 3) {
                        std::size_t ptIds[3];
                        std::array<float, 3> vertex;
                        for (std::size_t vi = 0; vi < 3; vi++) {// insert triangle
                            const auto vert = edges[edge[vi]];
                            // for discrete marching cubes, the interpolation point is always 0.5.
                            const auto t = 0.5;
                            const auto x1 = pts[vert[0]];
                            const auto x2 = pts[vert[1]];
                            vertex[0] = x1[0] + t * (x2[0] - x1[0]);
                            vertex[1] = x1[1] + t * (x2[1] - x1[1]);
                            vertex[2] = x1[2] + t * (x2[2] - x1[2]);

                            // add point
                            if (points.find({vertex[0], vertex[1], vertex[2]}) == std::end(points)) {
                                points[{vertex[0], vertex[1], vertex[2]}] = ptIds[vi] = idCounter++;
                            } else {
                                ptIds[vi] = points[{vertex[0], vertex[1], vertex[2]}];
                            }
                        }
                        if (ptIds[0] != ptIds[1] && ptIds[0] != ptIds[2] && ptIds[1] != ptIds[2] ) {// check for degenerate triangle
                            faces.push_back(ptIds[0]);
                            faces.push_back(ptIds[1]);
                            faces.push_back(ptIds[2]);
                        }
                    }
                }
            }
        }
        qDebug() <<  points.size() << faces.size() / 3;
    }
    QVector<float> verts(3 * points.size());
    for (auto && pair : points) {
        verts[3 * pair.second] = pair.first.x;
        verts[3 * pair.second + 1] = pair.first.y;
        verts[3 * pair.second + 2] = pair.first.z;
    }
    QVector<float> normals;
    QVector<std::uint8_t> colors;
    Skeletonizer::singleton().addMeshToTree(value, verts, normals, faces, colors, GL_TRIANGLES);
}
