/*
 *  This file is a part of KNOSSOS.
 *
 *  (C) Copyright 2007-2018
 *  Max-Planck-Gesellschaft zur Foerderung der Wissenschaften e.V.
 *
 *  KNOSSOS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 of
 *  the License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *  For further information, visit https://knossostool.org
 *  or contact knossos-team@mpimf-heidelberg.mpg.de
 */

#ifndef SEGMENTATION_H
#define SEGMENTATION_H

#include "coordinate.h"
#include "hash_list.h"
#include "segmentationsplit.h"

#include <QColor>
#include <QDebug>
#include <QSet>
#include <QString>
#include <QObject>

#include <array>
#include <atomic>
#include <boost/optional.hpp>
#include <functional>
#include <random>
#include <tuple>
#include <unordered_map>
#include <vector>

class Segmentation : public QObject {
Q_OBJECT
    template<typename T, typename U>
    friend void marchingCubes(std::unordered_map<U, std::unordered_map<floatCoordinate, int>> & obj2points, std::unordered_map<U, std::vector<unsigned int>> & obj2faces, std::unordered_map<std::uint64_t, std::size_t> & obj2idCounter, const std::vector<T> & data, const std::unordered_map<T, U> & soid2oid, const std::array<double, 3> & origin, const std::array<double, 3> & dims, const std::array<double, 3> & spacing, const std::array<double, 6> & extent);
    friend void connectedComponent(const Coordinate & seed);
    friend void generateMeshesForSubobjectsOfSelectedObjects();
    friend void verticalSplittingPlane(const Coordinate & seed);
    friend auto & objectFromId(const quint64 objId);
    friend class SegmentationObjectModel;
    friend class TouchedObjectModel;
    friend class CategoryDelegate;
    friend class CategoryModel;
    friend class SegmentationView;
    friend class SegmentationProxy;

    class Object;
    class SubObject {
        template<typename T, typename U>
        friend void marchingCubes(std::unordered_map<U, std::unordered_map<floatCoordinate, int>> & obj2points, std::unordered_map<U, std::vector<unsigned int>> & obj2faces, std::unordered_map<std::uint64_t, std::size_t> & obj2idCounter, const std::vector<T> & data, const std::unordered_map<T, U> & soid2oid, const std::array<double, 3> & origin, const std::array<double, 3> & dims, const std::array<double, 3> & spacing, const std::array<double, 6> & extent);
        friend void connectedComponent(const Coordinate & seed);
        friend void verticalSplittingPlane(const Coordinate & seed);
        friend class SegmentationObjectModel;
        friend class Segmentation;
        static uint64_t highestId;
        std::vector<uint64_t> objects;
        std::size_t selectedObjectsCount = 0;
    public:
        const uint64_t id;
        explicit SubObject(const uint64_t & id) : id(id) {
            highestId = std::max(id, highestId);
            objects.reserve(10);//improves merging performance by a factor of 3
        }
        SubObject(SubObject &&) = delete;
        SubObject(const SubObject &) = delete;
    };

    template<typename T, typename U>
    friend bool operator<(const std::reference_wrapper<T> & lhs, const std::reference_wrapper<U> & rhs) {
        return lhs.get().id < rhs.get().id;
    }
    template<typename T, typename U>
    friend bool operator==(const std::reference_wrapper<T> & lhs, const std::reference_wrapper<U> & rhs) {
        return lhs.get().id == rhs.get().id;
    }

    class Object {
        friend void connectedComponent(const Coordinate & seed);
        friend void verticalSplittingPlane(const Coordinate & seed);
        friend class SegmentationObjectModel;
        friend class TouchedObjectModel;
        friend class SegmentationView;
        friend class SegmentationProxy;
        friend class Segmentation;

        static uint64_t highestId;
        static uint64_t highestIndex;
    public:
        //see http://coliru.stacked-crooked.com/a/aba85777991b4425
        std::vector<std::reference_wrapper<SubObject>> subobjects;
        uint64_t id;
        uint64_t index = ++highestIndex;
        bool todo;
        bool immutable;
        Coordinate location;
        QString category;
        boost::optional<std::tuple<uint8_t, uint8_t, uint8_t>> color;
        QString comment;
        bool selected = false;

        explicit Object(std::vector<std::reference_wrapper<SubObject>> initialVolumes, const Coordinate & location, const uint64_t id = ++highestId, const bool & todo = false, const bool & immutable = false);
        explicit Object(Object & first, Object & second);
        bool operator==(const Object & other) const;
        void addExistingSubObject(SubObject & sub);
        Object & merge(Object & other);
    };

    std::unordered_map<uint64_t, SubObject> subobjects;
    std::vector<Object> objects;
    std::unordered_map<uint64_t, uint64_t> objectIdToIndex;
    hash_list<uint64_t> selectedObjectIndices;
    const QSet<QString> prefixed_categories = {"", "ecs", "mito", "myelin", "neuron", "synapse"};
    QSet<QString> categories = prefixed_categories;
    uint64_t backgroundId = 0;
    uint64_t hovered_subobject_id = 0;
    // Selection via subobjects touches all objects containing the subobject.
    uint64_t touched_subobject_id = 0;
    // For segmentation job mode
    uint64_t lastTodoObject_id = 0;

    // This array holds the table for overlay coloring.
    // The colors should be "maximally different".
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> overlayColorMap;

    Object & createObjectFromSubobjectId(const uint64_t initialSubobjectId, const Coordinate & location, uint64_t objectId = ++Object::highestId, const bool todo = false, const bool immutable = false);
    template<typename... Args>
    Object & createObject(Args && ... args);
    void removeObject(Object &);
    void changeCategory(Object & obj, const QString & category);
    void changeColor(Object & obj, const std::tuple<uint8_t, uint8_t, uint8_t> & color);
    void changeComment(Object & obj, const QString & comment);
    void newSubObject(Object & obj, uint64_t subObjID);

    std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> subobjectColor(const uint64_t subObjectID) const;

    void unmergeObject(Object & object, Object & other, const Coordinate & position);

    Object & objectFromSubobject(Segmentation::SubObject & subobject, const Coordinate & position);
public:
    class Job {
    public:
        bool active = false;
        int id;
        QString campaign;
        QString worker;
        QString path;
        QString submitPath;
    };
    Job job;
    void jobLoad(QIODevice & file);
    void jobSave(QIODevice & file) const;
    void startJobMode();
    void selectNextTodoObject();
    void selectPrevTodoObject();
    void markSelectedObjectForSplitting(const Coordinate & pos);

    bool renderOnlySelectedObjs{false};
    uint8_t alpha;
    brush_subject brush;
    // for mode in which edges are online highlighted for objects when selected and being hovered over by mouse
    bool hoverVersion{false};
    uint64_t mouseFocusedObjectId{0};
    bool highlightBorder{true}; //highlight borders in segmentation viewport

    bool enabled{false};
    std::size_t layerId;

    static Segmentation & singleton();

    Segmentation();
    //rendering
    void setRenderOnlySelectedObjs(const bool onlySelected);
    decltype(backgroundId) getBackgroundId() const;
    void setBackgroundId(decltype(backgroundId));
    std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> colorObjectFromIndex(const uint64_t objectIndex) const;
    std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> colorOfSelectedObject() const;
    std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> colorOfSelectedObject(const SubObject & subobject) const;
    std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> colorObjectFromSubobjectId(const uint64_t subObjectID) const;
    //volume rendering
    bool volume_render_toggle = false;
    std::atomic_bool volume_update_required{false};
    uint volume_tex_id = 0;
    int volume_tex_len = 128;
    int volume_mouse_move_x = 0;
    int volume_mouse_move_y = 0;
    float volume_mouse_zoom = 1.0f;
    uint8_t volume_opacity = 255;
    QColor volume_background_color{Qt::darkGray};
    //data query
    bool hasObjects() const;
    bool hasSegData() const;
    bool subobjectExists(const uint64_t & subobjectId) const;
    //data access
    void createAndSelectObject(const Coordinate & position);
    SubObject & subobjectFromId(const uint64_t & subobjectId, const Coordinate & location);
    uint64_t subobjectIdOfFirstSelectedObject(const Coordinate & newLocation);
    bool objectOrder(const uint64_t &lhsIndex, const uint64_t &rhsIndex) const;
    uint64_t largestObjectContainingSubobjectId(const uint64_t subObjectId, const Coordinate & location);
    uint64_t largestObjectContainingSubobject(const SubObject & subobject) const;
    uint64_t tryLargestObjectContainingSubobject(const uint64_t subObjectId) const;
    uint64_t smallestImmutableObjectContainingSubobject(const SubObject & subobject) const;
    //selection query
    bool isSelected(const SubObject & rhs) const;
    bool isSelected(const uint64_t &objectIndex) const;
    bool isSubObjectIdSelected(const uint64_t & subobjectId) const;
    std::size_t selectedObjectsCount() const;
    //selection modification
    void selectObject(const uint64_t & objectIndex, const boost::optional<Coordinate> position = boost::none);
    void selectObject(Object & object);
    void selectObjectFromSubObject(SubObject &subobject, const Coordinate & position);
    void selectObjectFromSubObject(const uint64_t soid, const Coordinate & position);
    void unselectObject(const uint64_t & objectIndex);
    void unselectObject(Object & object);
    void clearObjectSelection();

    void setObjectLocation(const uint64_t, const Coordinate & position);

    void jumpToObject(const uint64_t & objectIndex);
    void jumpToObject(Object & object);
    std::vector<std::reference_wrapper<Segmentation::Object>> todolist();

    void hoverSubObject(const uint64_t subobject_id);
    void touchObjects(const uint64_t subobject_id);
    void untouchObjects();
    std::vector<std::reference_wrapper<Object>> touchedObjects();
    //files
    void mergelistClear();
    void mergelistSave(QIODevice & file) const;
    void mergelistSave(QTextStream & stream) const;
    void mergelistLoad(QIODevice & file);
    void mergelistLoad(QTextStream & stream);
    void loadOverlayLutFromFile(const QString & filename = ":/resources/color_palette/default.json");
signals:
    void beforeAppendRow();
    void beforeRemoveRow();
    void appendedRow();
    void removedRow();
    void changedRow(int index);
    void changedRowSelection(int index);
    void merged(const quint64 index1, const quint64 index2); // used in plugin dfs_merge
    void unmerged(const quint64 index1, const quint64 index2); // used in plugin dfs_merge
    void selectionChanged();
    void resetData();
    void resetSelection();
    void resetTouchedObjects();
    void renderOnlySelectedObjsChanged(bool onlySelected);
    void backgroundIdChanged(uint64_t backgroundId);
    void categoriesChanged();
    void todosLeftChanged();
    void hoveredSubObjectChanged(const uint64_t subobject_id, const std::vector<uint64_t> & overlapObjects);
    void volumeRenderToggled(const bool render);
public slots:
    void clear();
    void deleteSelectedObjects();
    void mergeSelectedObjects();
    void unmergeSelectedObjects(const Coordinate & clickPos);
    void jumpToSelectedObject();
    bool placeCommentForSelectedObject(const QString & comment);
    void restoreDefaultColorForSelectedObjects();
    void toggleVolumeRender(const bool render);
};

#endif // SEGMENTATION_H
