#include "skeletonview.h"

#include "model_helper.h"
#include "session.h"
#include "skeleton/node.h"
#include "skeleton/skeletonizer.h"
#include "skeleton/tree.h"

#include <QCheckBox>
#include <QInputDialog>
#include <QMessageBox>

template<typename Func>
void question(QWidget * const parent, Func func, const QString & acceptButtonText, const QString & text, const QString & extraText, const bool condition = true) {
    if (condition) {
        QMessageBox prompt(parent);
        prompt.setIcon(QMessageBox::Question);
        prompt.setText(text);
        prompt.setInformativeText(extraText);
        const auto & confirmButton = *prompt.addButton(acceptButtonText, QMessageBox::AcceptRole);
        prompt.addButton(QMessageBox::Cancel);
        prompt.exec();
        if (prompt.clickedButton() == &confirmButton) {
            func();
        }
    }
}

template<typename ConcreteModel>
int AbstractSkeletonModel<ConcreteModel>::columnCount(const QModelIndex &) const {
    return static_cast<ConcreteModel const * const>(this)->header.size();
}
template<typename ConcreteModel>
QVariant AbstractSkeletonModel<ConcreteModel>::headerData(int section, Qt::Orientation orientation, int role) const {
    return (orientation == Qt::Horizontal && role == Qt::DisplayRole) ? static_cast<ConcreteModel const * const>(this)->header[section] : QVariant();
}
template<typename ConcreteModel>
Qt::ItemFlags AbstractSkeletonModel<ConcreteModel>::flags(const QModelIndex &index) const {
    return QAbstractItemModel::flags(index) | Qt::ItemNeverHasChildren | static_cast<ConcreteModel const * const>(this)->flagModifier[index.column()];
}
template<typename ConcreteModel>
int AbstractSkeletonModel<ConcreteModel>::rowCount(const QModelIndex &) const {
    return static_cast<ConcreteModel const * const>(this)->cache.size();
}

template class AbstractSkeletonModel<TreeModel>;//please clang, should actually be implicitly instantiated in here anyway

QVariant TreeModel::data(const QModelIndex &index, int role) const {
    const auto & tree = cache[index.row()].get();

    if (index.column() == 2 && role == Qt::BackgroundRole) {
        return QColor(tree.color.r * 255, tree.color.g * 255, tree.color.b * 255, tree.color.a * 255);
    } else if (index.column() == 4 && role == Qt::CheckStateRole) {
        return tree.render ? Qt::Checked : Qt::Unchecked;
    } else if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case 0: return tree.treeID;
        case 1: return tree.comment;
        case 3: return static_cast<quint64>(tree.nodes.size());
        }
    }
    return QVariant();//return invalid QVariant
}

bool TreeModel::setData(const QModelIndex & index, const QVariant & value, int role) {
    if (!index.isValid()) {
        return false;
    }
    auto & tree = cache[index.row()].get();

    if (index.column() == 4 && role == Qt::CheckStateRole) {
        tree.render = value.toBool();
    } else if ((role == Qt::DisplayRole || role == Qt::EditRole) && index.column() == 2) {
        const QColor color{value.value<QColor>()};
        tree.color = {static_cast<float>(color.redF()), static_cast<float>(color.greenF()), static_cast<float>(color.blueF()), static_cast<float>(color.alphaF())};
    } else if ((role == Qt::DisplayRole || role == Qt::EditRole) && index.column() == 1) {
        const QString comment{value.toString()};
        Skeletonizer::singleton().addTreeComment(tree.treeID, comment);
    } else {
        return false;
    }
    return true;
}

QVariant NodeModel::data(const QModelIndex &index, int role) const {
    if (state->skeletonState->trees.empty()) {
        return QVariant();//return invalid QVariant
    }
    const auto & node = cache[index.row()].get();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case 0: return static_cast<quint64>(node.nodeID);
        case 1: return node.comment != nullptr ? node.comment->content : "";
        case 2: return node.position.x;
        case 3: return node.position.y;
        case 4: return node.position.z;
        case 5: return node.radius;
        }
    }
    return QVariant();//return invalid QVariant
}

bool NodeModel::setData(const QModelIndex & index, const QVariant & value, int role) {
    if (state->skeletonState->trees.empty() || !index.isValid() || !(role == Qt::DisplayRole || role == Qt::EditRole)) {
        return false;
    }
    auto & node = cache[index.row()].get();

    if (index.column() == 1) {
        const QString comment{value.toString()};
        Skeletonizer::singleton().setComment(node, comment);
    } else if (index.column() == 2) {
        const Coordinate position{value.toInt(), node.position.y, node.position.z};
        Skeletonizer::singleton().editNode(0, &node, node.radius, position, node.createdInMag);
    } else if (index.column() == 3) {
        const Coordinate position{node.position.x, value.toInt(), node.position.z};
        Skeletonizer::singleton().editNode(0, &node, node.radius, position, node.createdInMag);
    } else if (index.column() == 4) {
        const Coordinate position{node.position.x, node.position.y, value.toInt()};
        Skeletonizer::singleton().editNode(0, &node, node.radius, position, node.createdInMag);
    } else if (index.column() == 5) {
        const float radius{value.toFloat()};
        Skeletonizer::singleton().editNode(0, &node, radius, node.position, node.createdInMag);
    } else {
        return false;
    }
    return true;
}

bool TreeModel::dropMimeData(const QMimeData *, Qt::DropAction, int, int, const QModelIndex & parent) {
    if (parent.isValid()) {
        emit moveNodes(parent);
        return true;
    }
    return false;
}

void TreeModel::recreate() {
    beginResetModel();
    cache.clear();
    for (auto && tree : state->skeletonState->trees) {
        cache.emplace_back(tree);
    }
    endResetModel();
}

void NodeModel::recreate() {
    beginResetModel();
    cache.clear();
    if (mode == ALL) {
        for (auto && tree : state->skeletonState->trees)
        for (auto && node : tree.nodes) {
            cache.emplace_back(node);
        }
    } else if (mode == PART_OF_SELECTED_TREE) {
        for (auto && tree : state->skeletonState->trees) {
            if (tree.selected) {
                for (auto && node : tree.nodes) {
                    cache.emplace_back(node);
                }
            }
        }
    } else if (mode == SELECTED) {
        for (auto && node : state->skeletonState->selectedNodes) {
            cache.emplace_back(*node);
        }
    } else if (mode == BRANCH) {
        for (auto && tree : state->skeletonState->trees)
        for (auto && node : tree.nodes) {
            if (node.isBranchNode) {
                cache.emplace_back(node);
            }
        }
    } else if (mode == NON_EMPTY_COMMENT) {
        for (auto && tree : state->skeletonState->trees)
        for (auto && node : tree.nodes) {
            if (node.comment != nullptr) {
                cache.emplace_back(node);
            }
        }
    }
    endResetModel();
}

void NodeView::mousePressEvent(QMouseEvent * event) {
    const auto index = proxy.mapToSource(indexAt(event->pos()));
    if (index.isValid()) {//enable drag’n’drop only for selected items to retain rubberband selection
        const auto selected = source.cache[index.row()].get().selected;
        setDragDropMode(selected ? QAbstractItemView::DragOnly : QAbstractItemView::NoDragDrop);
    }
    QTreeView::mousePressEvent(event);
}

template<typename T, typename Model, typename Proxy>
auto selectElems(Model & model, Proxy & proxy) {
    return [&model, &proxy](const QItemSelection & selected, const QItemSelection & deselected){
        if (!model.selectionProtection) {
            const auto & proxySelected = proxy.mapSelectionToSource(selected);
            const auto & proxyDeselected = proxy.mapSelectionToSource(deselected);
            auto indices = proxySelected.indexes();
            indices.append(proxyDeselected.indexes());
            QSet<T*> elems;
            for (const auto & modelIndex : indices) {
                if (modelIndex.column() == 0) {
                    elems.insert(&model.cache[modelIndex.row()].get());
                }
            }
            Skeletonizer::singleton().toggleSelection(elems);
        }
    };
}

template<typename Model, typename Proxy>
auto updateSelection(QTreeView & view, Model & model, Proxy & proxy) {
    const auto selectedIndices = proxy.mapSelectionFromSource(blockSelection(model, model.cache));
    model.selectionProtection = true;
    view.selectionModel()->select(selectedIndices, QItemSelectionModel::ClearAndSelect);
    model.selectionProtection = false;

    if (!selectedIndices.indexes().isEmpty()) {// scroll to first selected entry
        view.scrollTo(selectedIndices.indexes().front());
    }
}

template<typename... Args>
void deleteAction(QMenu & menu, QTreeView & view, QString text, Args &&... args) {
    auto * deleteAction = menu.addAction(QIcon(":/resources/icons/user-trash.png"), text);
    QObject::connect(deleteAction, &QAction::triggered, args...);
    view.addAction(deleteAction);
    deleteAction->setShortcut(Qt::Key_Delete);
    deleteAction->setShortcutContext(Qt::WidgetShortcut);
}

SkeletonView::SkeletonView(QWidget * const parent) : QWidget{parent}, nodeView{nodeSortAndCommentFilterProxy, nodeModel} {
    auto setupTable = [this](auto & table, auto & model, auto & sortIndex){
        table.setModel(&model);
        table.setUniformRowHeights(true);//perf hint from doc
        table.setSelectionMode(QAbstractItemView::ExtendedSelection);
        table.setSortingEnabled(true);
        table.sortByColumn(sortIndex = 0, Qt::SortOrder::AscendingOrder);
    };

    treeCommentFilter.setPlaceholderText("tree comment");

    treeSortAndCommentFilterProxy.setFilterKeyColumn(1);
    treeSortAndCommentFilterProxy.setSourceModel(&treeModel);
    setupTable(treeView, treeSortAndCommentFilterProxy, treeSortSectionIndex);
    treeView.setDragDropMode(QAbstractItemView::DropOnly);
    treeView.setDropIndicatorShown(true);

    displayModeCombo.addItems({"all", "from selected trees", "only selected", "branch", "comment"});
    displayModeCombo.setCurrentIndex(nodeModel.mode);
    nodeCommentFilter.setPlaceholderText("node comment");

    nodeSortAndCommentFilterProxy.setSourceModel(&nodeModel);
    nodeSortAndCommentFilterProxy.setFilterKeyColumn(1);
    setupTable(nodeView, nodeSortAndCommentFilterProxy, nodeSortSectionIndex);

    treeOptionsLayout.addWidget(&treeCommentFilter);
    treeOptionsLayout.addWidget(&treeRegex);
    treeLayout.addLayout(&treeOptionsLayout);
    treeLayout.addWidget(&treeView);
    treeDummyWidget.setLayout(&treeLayout);
    splitter.addWidget(&treeDummyWidget);

    nodeOptionsLayout.addWidget(&displayModeCombo);
    nodeOptionsLayout.addWidget(&nodeCommentFilter);
    nodeOptionsLayout.addWidget(&nodeRegex);
    nodeLayout.addLayout(&nodeOptionsLayout);
    nodeLayout.addWidget(&nodeView);
    nodeDummyWidget.setLayout(&nodeLayout);
    splitter.addWidget(&nodeDummyWidget);

    splitter.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);//size policy of QTreeView is also Expanding
    bottomHLayout.addWidget(&treeCountLabel);
    bottomHLayout.addWidget(&nodeCountLabel);
    mainLayout.addWidget(&splitter);
    mainLayout.addLayout(&bottomHLayout);
    setLayout(&mainLayout);

    static auto updateTreeSelection = [this](){
        updateSelection(treeView, treeModel, treeSortAndCommentFilterProxy);
    };
    static auto updateNodeSelection = [this](){
        updateSelection(nodeView, nodeModel, nodeSortAndCommentFilterProxy);
    };
    static auto treeRecreate = [&, this](){
        treeView.selectionModel()->reset();
        treeModel.recreate();
        updateTreeSelection();
    };
    static auto nodeRecreate = [&, this](){
        nodeView.selectionModel()->reset();
        nodeModel.recreate();
        updateNodeSelection();
    };
    static auto allRecreate = [&](){
        treeRecreate();
        nodeRecreate();
    };

    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::treeAddedSignal, allRecreate);
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::treeChangedSignal, treeRecreate);
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::treeRemovedSignal, allRecreate);
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::treesMerged, treeRecreate);
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::treeSelectionChangedSignal, [this](){
        updateTreeSelection();
        if (nodeModel.mode == NodeModel::PART_OF_SELECTED_TREE) {
            nodeRecreate();
        }
    });

    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::branchPoppedSignal, nodeRecreate);
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::branchPushedSignal, nodeRecreate);
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::nodeSelectionChangedSignal, [this](){
        updateNodeSelection();
        if (nodeModel.mode == NodeModel::SELECTED) {
            nodeRecreate();
        }
    });

    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::nodeAddedSignal, nodeRecreate);
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::nodeChangedSignal, nodeRecreate);
    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::nodeRemovedSignal, nodeRecreate);

    QObject::connect(&Skeletonizer::singleton(), &Skeletonizer::resetData, allRecreate);

    static auto filter = [](auto & regex, auto & model, const QString & filterText){
        if (regex.isChecked()) {
            model.setFilterRegExp(filterText);
        } else {
            model.setFilterFixedString(filterText);
        }
    };
    QObject::connect(&treeCommentFilter, &QLineEdit::textEdited, [this](const QString & filterText){
        filter(treeRegex, treeSortAndCommentFilterProxy, filterText);
        updateTreeSelection();
    });

    QObject::connect(&displayModeCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int index){
        nodeModel.mode = static_cast<decltype(nodeModel.mode)>(index);
        nodeRecreate();
    });
    QObject::connect(&nodeCommentFilter, &QLineEdit::textEdited, [this](const QString & filterText){
        filter(nodeRegex, nodeSortAndCommentFilterProxy, filterText);
        updateNodeSelection();
    });

    QObject::connect(&treeModel, &TreeModel::moveNodes, [this](const QModelIndex & parent){
        const auto index = parent.row();//already is a source model index
        const auto droppedOnTreeID = treeModel.cache[index].get().treeID;
        const auto text = tr("Do you really want to move selected nodes to tree %1?").arg(droppedOnTreeID);
        question(this, [droppedOnTreeID](){Skeletonizer::singleton().moveSelectedNodesToTree(droppedOnTreeID);}, tr("Move"), text, tr(""));
    });

    QObject::connect(treeView.selectionModel(), &QItemSelectionModel::selectionChanged, selectElems<treeListElement>(treeModel, treeSortAndCommentFilterProxy));
    QObject::connect(nodeView.selectionModel(), &QItemSelectionModel::selectionChanged, selectElems<nodeListElement>(nodeModel, nodeSortAndCommentFilterProxy));

    QObject::connect(treeView.header(), &QHeaderView::sortIndicatorChanged, threeWaySorting(treeView, treeSortSectionIndex));
    QObject::connect(nodeView.header(), &QHeaderView::sortIndicatorChanged, threeWaySorting(nodeView, nodeSortSectionIndex));

    treeView.setContextMenuPolicy(Qt::CustomContextMenu);//enables signal for custom context menu
    QObject::connect(&treeView, &QTreeView::customContextMenuRequested, [this](const QPoint & pos){
        int i = 0;
        treeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedTrees.size() == 1);//show
        treeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedTrees.size() == 1 && state->skeletonState->selectedNodes.size() > 0);//move nodes
        treeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedTrees.size() >= 2);//merge trees action
        treeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedTrees.size() > 0);//show
        treeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedTrees.size() > 0);//hide
        treeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedTrees.size() > 0);//restore default color
        //display the context menu at pos in screen coordinates instead of widget coordinates of the content of the currently focused table
        treeContextMenu.exec(treeView.viewport()->mapToGlobal(pos));
    });
    nodeView.setContextMenuPolicy(Qt::CustomContextMenu);//enables signal for custom context menu
    QObject::connect(&nodeView, &QTreeView::customContextMenuRequested, [this](const QPoint & pos){
        int i = 0;
        nodeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedNodes.size() == 1);//jump to node
        nodeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedNodes.size() == 1);//split connected components
        nodeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedNodes.size() == 2);//link nodes needs two selected nodes
        nodeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedNodes.size() > 0);//set comment
        nodeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedNodes.size() > 0);//set radius
        nodeContextMenu.actions().at(i++)->setEnabled(state->skeletonState->selectedNodes.size() > 0);//delete
        //display the context menu at pos in screen coordinates instead of widget coordinates of the content of the currently focused table
        nodeContextMenu.exec(nodeView.viewport()->mapToGlobal(pos));
    });


    QObject::connect(treeContextMenu.addAction("&Jump to first node"), &QAction::triggered, [this](){
        const auto * tree = state->skeletonState->selectedTrees.front();
        if (!tree->nodes.empty()) {
            Skeletonizer::singleton().jumpToNode(tree->nodes.front());
        }
    });
    QObject::connect(treeContextMenu.addAction("Move selected nodes to this tree"), &QAction::triggered, [this](){
        const auto treeID = state->skeletonState->selectedTrees.front()->treeID;
        const auto text = tr("Do you really want to move selected nodes to tree %1?").arg(treeID);
        question(this, [treeID](){ Skeletonizer::singleton().moveSelectedNodesToTree(treeID); }, tr("Move"), text, tr(""));
    });
    QObject::connect(treeContextMenu.addAction("&Merge trees"), &QAction::triggered, [this](){
        question(this, [](){
            auto initiallySelectedTrees = state->skeletonState->selectedTrees;//HACK mergeTrees clears the selection by setting the merge result active
            while (initiallySelectedTrees.size() >= 2) {//2 at a time
                const int treeID1 = initiallySelectedTrees[0]->treeID;
                const int treeID2 = initiallySelectedTrees[1]->treeID;
                initiallySelectedTrees.erase(std::begin(initiallySelectedTrees)+1);//HACK mergeTrees deletes tree2
                Skeletonizer::singleton().mergeTrees(treeID1, treeID2);
            }
        }, tr("Merge"), tr("Do you really want to merge all selected trees?"), tr(""));
    });
    QObject::connect(treeContextMenu.addAction("Set &comment for trees"), &QAction::triggered, [this](){
        bool applied = false;
        static auto prevComment = QString("");
        auto comment = QInputDialog::getText(this, "Edit Tree Comment", "new tree comment", QLineEdit::Normal, prevComment, &applied);
        if (applied) {
            prevComment = comment;
            Skeletonizer::singleton().addTreeCommentToSelectedTrees(comment);
        }
    });
    QObject::connect(treeContextMenu.addAction("Show selected trees"), &QAction::triggered, [](){
        for (auto * tree : state->skeletonState->selectedTrees) {
            tree->render = true;
        }
    });
    QObject::connect(treeContextMenu.addAction("Hide selected trees"), &QAction::triggered, [](){
        for (auto * tree : state->skeletonState->selectedTrees) {
            tree->render = false;
        }});
    QObject::connect(treeContextMenu.addAction("Restore default color"), &QAction::triggered, [](){
        for (auto * tree : state->skeletonState->selectedTrees) {
            Skeletonizer::singleton().restoreDefaultTreeColor(*tree);
        }
    });
    deleteAction(treeContextMenu, treeView, tr("&Delete trees"), [this](){
        question(this, [](){ Skeletonizer::singleton().deleteSelectedTrees(); }, tr("Delete"), tr("Delete the selected trees?"), tr(""), !state->skeletonState->selectedTrees.empty());
    });


    QObject::connect(nodeContextMenu.addAction("&Jump to node"), &QAction::triggered, [this](){
        Skeletonizer::singleton().jumpToNode(*state->skeletonState->selectedNodes.front());
    });
    QObject::connect(nodeContextMenu.addAction("&Extract connected component"), &QAction::triggered, [this](){
        auto res = QMessageBox::Ok;
        static bool askExtractConnectedComponent = true;
        if (askExtractConnectedComponent) {
            const auto msg = tr("Do you really want to extract all nodes in this component into a new tree?");

            QMessageBox msgBox(this);
            msgBox.setIcon(QMessageBox::Question);
            msgBox.setText(msg);
            msgBox.addButton(QMessageBox::Ok);
            msgBox.addButton(QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Cancel);
            QCheckBox dontShowCheckBox(tr("Hide message until restart"));
            dontShowCheckBox.blockSignals(true);
            msgBox.addButton(&dontShowCheckBox, QMessageBox::ResetRole);
            res = static_cast<QMessageBox::StandardButton>(msgBox.exec());
            askExtractConnectedComponent = dontShowCheckBox.checkState() == Qt::Unchecked;
        }
        if (res == QMessageBox::Ok) {
            const auto treeID = state->skeletonState->selectedTrees.front()->treeID;
            const bool extracted = Skeletonizer::singleton().extractConnectedComponent(state->skeletonState->selectedNodes.front()->nodeID);
            if (!extracted) {
                QMessageBox::information(this, "Nothing to extract", "The component spans an entire tree.");
            } else {
                auto msg = tr("Extracted from %1").arg(treeID);
                if (Skeletonizer::singleton().findTreeByTreeID(treeID) == nullptr) {
                    msg = tr("Extracted from deleted %1").arg(treeID);
                }
                Skeletonizer::singleton().addTreeComment(state->skeletonState->trees.back().treeID, msg);
            }
        }
    });
    QObject::connect(nodeContextMenu.addAction("&Link/Unlink nodes"), &QAction::triggered, [this](){
        Skeletonizer::singleton().toggleConnectionOfFirstPairOfSelectedNodes(this);
    });
    QObject::connect(nodeContextMenu.addAction("Set &comment for nodes"), &QAction::triggered, [this](){
        bool applied = false;
        static auto prevComment = QString("");
        auto comment = QInputDialog::getText(this, "Edit Node Comment", "new node comment", QLineEdit::Normal, prevComment, &applied);
        if (applied) {
            prevComment = comment;
            for (auto node : state->skeletonState->selectedNodes) {
                Skeletonizer::singleton().setComment(*node,comment);
            }
        }
    });
    QObject::connect(nodeContextMenu.addAction("Set &radius for nodes"), &QAction::triggered, [this](){
        bool applied = false;
        static auto prevRadius = Skeletonizer::singleton().skeletonState.defaultNodeRadius;
        auto radius = QInputDialog::getDouble(this, "Edit Node Radii", "new node radius", prevRadius, 0, 100000, 1, &applied);
        if (applied) {
            prevRadius = radius;
            for (auto * node : state->skeletonState->selectedNodes) {
                Skeletonizer::singleton().editNode(0, node, radius, node->position, node->createdInMag);
            }
        }
    });
    deleteAction(nodeContextMenu, nodeView, tr("&Delete nodes"), [this](){
        if (state->skeletonState->selectedNodes.size() == 1) {//don’t ask for one node
            Skeletonizer::singleton().deleteSelectedNodes();
        } else {
            question(this, [](){ Skeletonizer::singleton().deleteSelectedNodes(); }, tr("Delete"), tr("Delete the selected nodes?"), tr(""));
        }
    });

    treeContextMenu.setDefaultAction(treeContextMenu.actions().front());
    QObject::connect(&treeView, &QAbstractItemView::doubleClicked, [this](const QModelIndex &){
        treeContextMenu.defaultAction()->trigger();
    });
    nodeContextMenu.setDefaultAction(nodeContextMenu.actions().front());
    QObject::connect(&nodeView, &QAbstractItemView::doubleClicked, [this](const QModelIndex &){
        nodeContextMenu.defaultAction()->trigger();
    });
}