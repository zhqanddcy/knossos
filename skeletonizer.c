/*
 *  This file is a part of KNOSSOS.
 *
 *  (C) Copyright 2007-2012
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
 */

/*
 * For further information, visit http://www.knossostool.org or contact
 *     Joergen.Kornfeld@mpimf-heidelberg.mpg.de or
 *     Fabian.Svara@mpimf-heidelberg.mpg.de
 */

/*
 *      Code for the skeletonizer.
 *
 *      Explain it.
 *
 *      Functions explained in the header file.
 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <SDL/SDL.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "knossos-global.h"
#include "skeletonizer.h"

extern struct stateInfo *tempConfig;
extern struct stateInfo *state;

uint32_t initSkeletonizer(struct stateInfo *state) {
    time_t curtime;
    struct tm *localtimestruct;

    if(state->skeletonState->skeletonDCnumber != tempConfig->skeletonState->skeletonDCnumber)
        state->skeletonState->skeletonDCnumber = tempConfig->skeletonState->skeletonDCnumber;

    //updateSkeletonState(state);

    //Create a new hash-table that holds the skeleton datacubes
    state->skeletonState->skeletonDCs = ht_new(state->skeletonState->skeletonDCnumber);
    if(state->skeletonState->skeletonDCs == HT_FAILURE) {
        LOG("Unable to create skeleton hash-table.");
        return FALSE;
    }

    state->skeletonState->skeletonRevision = 0;

    state->skeletonState->nodeCounter = newDynArray(128);
    state->skeletonState->nodesByNodeID = newDynArray(1048576);
    state->skeletonState->branchStack = newStack(2048);

    // Generate empty tree structures
    state->skeletonState->firstTree = NULL;
    state->skeletonState->treeElements = 0;
    state->skeletonState->totalNodeElements = 0;
    state->skeletonState->totalSegmentElements = 0;
    state->skeletonState->activeTree = NULL;
    state->skeletonState->activeNode = NULL;

    state->skeletonState->mergeOnLoadFlag = 0;
    state->skeletonState->segRadiusToNodeRadius = 0.5;
    state->skeletonState->autoFilenameIncrementBool = TRUE;
    state->skeletonState->greatestNodeID = 0;

    state->skeletonState->showXYplane = TRUE;
    state->skeletonState->showXZplane = TRUE;
    state->skeletonState->showYZplane = TRUE;
    state->skeletonState->showNodeIDs = FALSE;
    state->skeletonState->highlightActiveTree = TRUE;
    state->skeletonState->rotateAroundActiveNode = TRUE;
    state->skeletonState->showIntersections = FALSE;

    state->skeletonState->displayListSkeletonSkeletonizerVP = 0;
    state->skeletonState->displayListView = 0;
    state->skeletonState->displayListDataset = 0;

    state->skeletonState->defaultNodeRadius = 1.5;
    state->skeletonState->overrideNodeRadiusBool = FALSE;
    state->skeletonState->overrideNodeRadiusVal = 1.;

    state->skeletonState->currentComment = NULL;

    state->skeletonState->lastSaveTicks = 0;
    state->skeletonState->autoSaveInterval = 5;

    state->skeletonState->skeletonFile = malloc(8192 * sizeof(char));
    memset(state->skeletonState->skeletonFile, '\0', 8192 * sizeof(char));

    /* Generate a default file name based on date and time. */
    curtime = time(NULL);
    localtimestruct = localtime(&curtime);
    if(localtimestruct->tm_year >= 100)
        localtimestruct->tm_year -= 100;

#ifdef LINUX
    snprintf(state->skeletonState->skeletonFile,
            8192,
            "skeletonFiles/skeleton-%.2d%.2d%.2d-%.2d%.2d.000.nml",
            localtimestruct->tm_mday,
            localtimestruct->tm_mon + 1,
            localtimestruct->tm_year,
            localtimestruct->tm_hour,
            localtimestruct->tm_min);
#else
    snprintf(state->skeletonState->skeletonFile,
            8192,
            "skeletonFiles\\skeleton-%.2d%.2d%.2d-%.2d%.2d.000.nml",
            localtimestruct->tm_mday,
            localtimestruct->tm_mon + 1,
            localtimestruct->tm_year,
            localtimestruct->tm_hour,
            localtimestruct->tm_min);
#endif

    state->skeletonState->commentBuffer = malloc(10240 * sizeof(char));
    memset(state->skeletonState->commentBuffer, '\0', 10240 * sizeof(char));

    state->skeletonState->searchStrBuffer = malloc(2048 * sizeof(char));
    memset(state->skeletonState->searchStrBuffer, '\0', 2048 * sizeof(char));

    state->skeletonState->saveCnt = 0;

    if((state->boundary.x >= state->boundary.y) && (state->boundary.x >= state->boundary.z))
        state->skeletonState->volBoundary = state->boundary.x * 2;
    if((state->boundary.y >= state->boundary.x) && (state->boundary.y >= state->boundary.y))
        state->skeletonState->volBoundary = state->boundary.y * 2;
    if((state->boundary.z >= state->boundary.x) && (state->boundary.z >= state->boundary.y))
        state->skeletonState->volBoundary = state->boundary.x * 2;

    state->skeletonState->rotateX = 0;
    state->skeletonState->rotateY = 0;
    state->skeletonState->rotateZ = 0;

    state->skeletonState->viewChanged = TRUE;
    state->skeletonState->skeletonChanged = TRUE;
    state->skeletonState->datasetChanged = TRUE;
    state->skeletonState->skeletonSliceVPchanged = TRUE;
    state->skeletonState->unsavedChanges = FALSE;

    state->skeletonState->askingPopBranchConfirmation = FALSE;

    return TRUE;
}

uint32_t updateSkeletonState(struct stateInfo *state) {
    //Time to auto-save
    if(state->skeletonState->autoSaveBool || state->clientState->saveMaster) {
        if(state->skeletonState->autoSaveInterval) {
            if((SDL_GetTicks() - state->skeletonState->lastSaveTicks) / 60000 >= state->skeletonState->autoSaveInterval) {
                state->skeletonState->lastSaveTicks = SDL_GetTicks();
                UI_saveSkeleton(TRUE);
            }
        }
    }

    if(state->skeletonState->skeletonDCnumber != tempConfig->skeletonState->skeletonDCnumber)
        state->skeletonState->skeletonDCnumber = tempConfig->skeletonState->skeletonDCnumber;

    if(state->skeletonState->workMode != tempConfig->skeletonState->workMode)
        setSkeletonWorkMode(CHANGE_MANUAL, tempConfig->skeletonState->workMode, state);
    return TRUE;
}

//update tree colors if preferences were changed.
int32_t updateTreeColors() {
    struct treeListElement *tree = state->skeletonState->firstTree;
    while(tree) {
        uint32_t index = state->viewerState->treeLutSize - 3;
        if(tree->treeID % (state->viewerState->treeLutSize/3) != 0) {
            index = (tree->treeID % (state->viewerState->treeLutSize/3) - 1) * 3;
        }
        tree->color.r = state->viewerState->treeAdjustmentTable[index];
        tree->color.g = state->viewerState->treeAdjustmentTable[index + 1];
        tree->color.b = state->viewerState->treeAdjustmentTable[index + 2];
        tree->color.a = 1.;
        tree = tree->next;
    }
    state->skeletonState->skeletonChanged = TRUE;
    return TRUE;
}

uint32_t setSkeletonWorkMode(int32_t targetRevision,
                             uint32_t workMode,
                             struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    state->skeletonState->workMode = workMode;
    tempConfig->skeletonState->workMode = workMode;

    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brd", KIKI_SETSKELETONMODE, workMode))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

static struct nodeListElement *addNodeListElement(
              int32_t nodeID,
              float radius,
              struct nodeListElement **currentNode,
              Coordinate *position,
              int32_t inMag,
              struct stateInfo *state) {

    struct nodeListElement *newElement = NULL;

    /*
     * This skeleton modifying function does not lock the skeleton and therefore
     * has to be called from functions that do lock and NEVER directly.
     *
     */

    newElement = malloc(sizeof(struct nodeListElement));
    if(newElement == NULL) {
        LOG("Out of memory while trying to allocate memory for a new nodeListElement.");
        return NULL;
    }
    memset(newElement, '\0', sizeof(struct nodeListElement));

    if((*currentNode) == NULL) {
        // Requested to add a node to a list that hasn't yet been started.
        *currentNode = newElement;
        newElement->previous = NULL;
        newElement->next = NULL;

    }
    else {
        newElement->previous = *currentNode;
        newElement->next = (*currentNode)->next;
        (*currentNode)->next = newElement;
        if(newElement->next)
            newElement->next->previous = newElement;
    }

    SET_COORDINATE(newElement->position, position->x, position->y, position->z);
    //Assign radius
    newElement->radius = radius;
    //Set node ID. This ID is unique in every tree list (there should only exist 1 tree list, see initSkeletonizer()).
    //Take the provided nodeID.
    newElement->nodeID = nodeID;
    newElement->isBranchNode = FALSE;
    newElement->createdInMag = inMag;

    return newElement;
}

int32_t addNode(int32_t targetRevision,
                int32_t nodeID,
                float radius,
                int32_t treeID,
                Coordinate *position,
                Byte VPtype,
                int32_t inMag,
                int32_t time,
                int32_t respectLocks,
                struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    struct nodeListElement *tempNode = NULL;
    struct treeListElement *tempTree = NULL;
    floatCoordinate lockVector;
    int32_t lockDistance = 0;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    state->skeletonState->branchpointUnresolved = FALSE;

    /*
     * respectLocks refers to locking the position to a specific coordinate such as to
     * disallow tracing areas further than a certain distance away from a specific point in the
     * dataset.
     */

    if(respectLocks) {
        if(state->skeletonState->positionLocked) {
            lockVector.x = (float)position->x - (float)state->skeletonState->lockedPosition.x;
            lockVector.y = (float)position->y - (float)state->skeletonState->lockedPosition.y;
            lockVector.z = (float)position->z - (float)state->skeletonState->lockedPosition.z;

            lockDistance = euclidicNorm(&lockVector);
            if(lockDistance > state->skeletonState->lockRadius) {
                LOG("Node is too far away from lock point (%d), not adding.", lockDistance);
                unlockSkeleton(FALSE, state);
                return FALSE;
            }
        }
    }

    if(nodeID)
        tempNode = findNodeByNodeID(nodeID, state);
    if(tempNode) {
        LOG("Node with ID %d already exists, no node added.", nodeID);
        unlockSkeleton(FALSE, state);
        return FALSE;
    }
    tempTree = findTreeByTreeID(treeID, state);

    if(!tempTree) {
        LOG("There exists no tree with the provided ID %d!", treeID);
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    // One node more in all trees
    state->skeletonState->totalNodeElements++;

    // One more node in this tree.
    setDynArray(state->skeletonState->nodeCounter,
                treeID,
                (void *)((PTRSIZEINT)getDynArray(state->skeletonState->nodeCounter, treeID) + 1));

    if(nodeID == 0) {
        nodeID = state->skeletonState->totalNodeElements;
        //Test if node ID over node counter is available. If not, find a valid one.
        while(findNodeByNodeID(nodeID, state)) {
            nodeID++;
        }
    }

    tempNode = addNodeListElement(nodeID, radius, &(tempTree->firstNode), position, inMag, state);
    tempNode->correspondingTree = tempTree;
    tempNode->comment = NULL;
    tempNode->createdInVp = VPtype;

    if(time == -1)
        time = state->skeletonState->skeletonTime - state->skeletonState->skeletonTimeCorrection + SDL_GetTicks();

    tempNode->timestamp = time;

    setDynArray(state->skeletonState->nodesByNodeID, nodeID, (void *)tempNode);

    //printf("Added node %p, id %d, tree %p\n", tempNode, tempNode->nodeID,
    //        tempNode->correspondingTree);

    //Add a pointer to the node in the skeleton DC structure
    addNodeToSkeletonStruct(tempNode, state);
    state->skeletonState->skeletonChanged = TRUE;

    if(nodeID > state->skeletonState->greatestNodeID)
        state->skeletonState->greatestNodeID = nodeID;

    state->skeletonState->skeletonRevision++;
    state->skeletonState->unsavedChanges = TRUE;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brddfddddddd", KIKI_ADDNODE,
                                              state->clientState->myId,
                                              nodeID,
                                              radius,
                                              treeID,
                                              VPtype,
                                              inMag,
                                              -1,
                                              position->x,
                                              position->y,
                                              position->z))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return nodeID;
}


int32_t editNode(int32_t targetRevision,
                 int32_t nodeID,
                 struct nodeListElement *node,
                 float newRadius,
                 int32_t newXPos,
                 int32_t newYPos,
                 int32_t newZPos,
                 int32_t inMag,
                 struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    struct segmentListElement *currentSegment = NULL;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    if(!node)
        node = findNodeByNodeID(nodeID, state);
    if(!node) {
        LOG("Cannot edit: node id %d invalid.", nodeID);
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    nodeID = node->nodeID;

    //Since the position can change, we have to rebuild the corresponding spatial skeleton structure
    delNodeFromSkeletonStruct(node, state);
    currentSegment = node->firstSegment;
    while(currentSegment) {
        delSegmentFromSkeletonStruct(currentSegment, state);
        currentSegment = currentSegment->next;
    }

    if(!((newXPos < 0) || (newXPos > state->boundary.x)
       || (newYPos < 0) || (newYPos > state->boundary.y)
       || (newZPos < 0) || (newZPos > state->boundary.z))) {
        SET_COORDINATE(node->position, newXPos, newYPos, newZPos);
    }

    if(newRadius != 0.)
        node->radius = newRadius;
    node->createdInMag = inMag;

    //Since the position can change, we have to rebuild the corresponding spatial skeleton structure
    addNodeToSkeletonStruct(node, state);
    currentSegment = node->firstSegment;
    while(currentSegment) {
        addSegmentToSkeletonStruct(currentSegment, state);
        currentSegment = currentSegment->next;
    }

    state->skeletonState->skeletonChanged = TRUE;
    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brddfdddd", KIKI_EDITNODE,
                                            state->clientState->myId,
                                            nodeID,
                                            newRadius,
                                            inMag,
                                            newXPos,
                                            newYPos,
                                            newZPos))

            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

uint32_t addSegment(int32_t targetRevision, int32_t sourceNodeID, int32_t targetNodeID, struct stateInfo *state) {
    struct nodeListElement *targetNode, *sourceNode;
    struct segmentListElement *sourceSeg;

    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    if(findSegmentByNodeIDs(sourceNodeID, targetNodeID, state)) {
        LOG("Segment between nodes %d and %d exists already.", sourceNodeID, targetNodeID);
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    //Check if source and target nodes are existant
    sourceNode = findNodeByNodeID(sourceNodeID, state);
    targetNode = findNodeByNodeID(targetNodeID, state);

    if(!(sourceNode) || !(targetNode)) {
        LOG("Could not link the nodes, because at least one is missing!");
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    if(sourceNode == targetNode) {
        LOG("Cannot link node with itself!");
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    //One segment more in all trees
    state->skeletonState->totalSegmentElements++;

    /*
     * Add the segment to the tree structure
    */

    sourceSeg = addSegmentListElement(&(sourceNode->firstSegment), sourceNode, targetNode, state);
    sourceSeg->reverseSegment = addSegmentListElement(&(targetNode->firstSegment), sourceNode, targetNode, state);

    sourceSeg->reverseSegment->flag = SEGMENT_BACKWARD;

    sourceSeg->reverseSegment->reverseSegment = sourceSeg;

    /*
     * Add the segment to the skeleton DC structure
    */

    addSegmentToSkeletonStruct(sourceSeg, state);

    // printf("added segment for nodeID %d: %d, %d, %d -> nodeID %d: %d, %d, %d\n", sourceNode->nodeID, sourceNode->position.x + 1, sourceNode->position.y + 1, sourceNode->position.z + 1, targetNode->nodeID, targetNode->position.x + 1, targetNode->position.y + 1, targetNode->position.z + 1);

    state->skeletonState->skeletonChanged = TRUE;
    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brdd", KIKI_ADDSEGMENT, sourceNodeID, targetNodeID))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}


uint32_t delSegment(int32_t targetRevision, int32_t sourceNodeID, int32_t targetNodeID, struct segmentListElement *segToDel, struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    /*
     * Delete the segment out of the segment list and out of the visualization structure!
     */

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    if(!segToDel)
        segToDel = findSegmentByNodeIDs(sourceNodeID, targetNodeID, state);
    else {
        sourceNodeID = segToDel->source->nodeID;
        targetNodeID = segToDel->target->nodeID;
    }

    if(!segToDel) {
        LOG("Cannot delete segment, no segment with corresponding node IDs available!");
        unlockSkeleton(FALSE, state);
        return FALSE;
    }


    //Out of skeleton structure
    delSegmentFromSkeletonStruct(segToDel, state);

    if(segToDel == segToDel->source->firstSegment)
        segToDel->source->firstSegment = segToDel->next;
    else {
        //if(segToDel->previous) //Why??? Previous EXISTS if its not the first seg...
            segToDel->previous->next = segToDel->next;
        if(segToDel->next)
            segToDel->next->previous = segToDel->previous;
    }

    //Delete reverse segment in target node
    if(segToDel->reverseSegment == segToDel->target->firstSegment)
        segToDel->target->firstSegment = segToDel->reverseSegment->next;
    else {
        //if(segToDel->reverseSegment->previous)
            segToDel->reverseSegment->previous->next = segToDel->reverseSegment->next;
        if(segToDel->reverseSegment->next)
            segToDel->reverseSegment->next->previous = segToDel->reverseSegment->previous;
    }

    free(segToDel);
    state->skeletonState->totalSegmentElements--;

    state->skeletonState->skeletonChanged = TRUE;
    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brdd", KIKI_DELSEGMENT, sourceNodeID, targetNodeID))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

struct treeListElement *addTreeListElement(int32_t sync, int32_t targetRevision, int32_t treeID, color4F color) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    /* The variable sync is a workaround for the problem that this function
     *  will sometimes be called by other syncable functions that themselves hold
     *  the lock and handle synchronization. If set to FALSE, all synchro
     *  routines will be skipped. */

    struct treeListElement *newElement = NULL;

    if(sync != FALSE) {
        if(lockSkeleton(targetRevision, state) == FALSE) {
            LOG("addtreelistelement unable to lock.");
            unlockSkeleton(FALSE, state);
            return FALSE;
        }
    }


    newElement = findTreeByTreeID(treeID, state);
    if(newElement) {
        LOG("Tree with ID %d already exists!", treeID);
        unlockSkeleton(TRUE, state);
        return newElement;
    }

    newElement = malloc(sizeof(struct treeListElement));
    if(newElement == NULL) {
        LOG("Out of memory while trying to allocate memory for a new treeListElement.");
        unlockSkeleton(FALSE, state);
        return NULL;
    }
    memset(newElement, '\0', sizeof(struct treeListElement));

    state->skeletonState->treeElements++;

    //Tree ID is unique in tree list
    //Take the provided tree ID if there is one.
    if(treeID != 0)
        newElement->treeID = treeID;
    else {
        newElement->treeID = state->skeletonState->treeElements;
        //Test if tree ID over tree counter is available. If not, find a valid one.
        while(findTreeByTreeID(newElement->treeID, state)) {
            newElement->treeID++;
        }
    }

    /* calling function sets values < 0 when no color was specified */
    if(color.r < 0 && state->viewerState->treeColortableOn) {//Set a tree color
        uint32_t index = state->viewerState->treeLutSize - 3;
        if(state->skeletonState->treeElements % (state->viewerState->treeLutSize/3) != 0) {
            index = (state->skeletonState->treeElements % (state->viewerState->treeLutSize/3) - 1) * 3;
        }
        newElement->color.r = state->viewerState->treeAdjustmentTable[index];
        newElement->color.g = state->viewerState->treeAdjustmentTable[index+1];
        newElement->color.b = state->viewerState->treeAdjustmentTable[index+2];
        newElement->color.a = 1.;
    }
    else if(color.r < 0 && state->viewerState->treeColortableOn == FALSE) {
        uint32_t index = state->viewerState->treeLutSize - 3;
        if(state->skeletonState->treeElements % (state->viewerState->treeLutSize/3) != 0) {
            index = (state->skeletonState->treeElements % (state->viewerState->treeLutSize/3) - 1) * 3;
        }
        newElement->color.r = state->viewerState->neutralTreeTable[index];
        newElement->color.g = state->viewerState->neutralTreeTable[index+1];
        newElement->color.b = state->viewerState->neutralTreeTable[index+2];
        newElement->color.a = 1.;
    }
    else {
        newElement->color = color;
    }
/* old buggy way of color assignment */
/*
        newElement->color.r = 0.5 / (float)(newElement->treeID % 5);
        newElement->color.g = 1. / (float)(newElement->treeID % 3);
        newElement->color.b = 1. / (float)(newElement->treeID % 4);
        newElement->color.a = 1.;
*/

    //Insert the new tree at the beginning of the tree list
    newElement->next = state->skeletonState->firstTree;
    newElement->previous = NULL;
    //The old first element should have the new first element as previous element
    if(state->skeletonState->firstTree)
        state->skeletonState->firstTree->previous = newElement;
    //We change the old and new first elements
    state->skeletonState->firstTree = newElement;

    state->skeletonState->activeTree = newElement;
    LOG("Added new tree with ID: %d.", newElement->treeID);

    if(treeID > state->skeletonState->greatestTreeID)
        state->skeletonState->greatestTreeID = treeID;

    state->skeletonState->skeletonChanged = TRUE;
    state->skeletonState->unsavedChanges = TRUE;

    if(sync != FALSE) {
        state->skeletonState->skeletonRevision++;

        if(targetRevision == CHANGE_MANUAL) {
            if(!syncMessage(state, "brd", KIKI_ADDTREE, newElement->treeID))
                skeletonSyncBroken(state);
        }

        unlockSkeleton(TRUE, state);
    }
    else
        refreshViewports(state);

    return newElement;
}

static struct segmentListElement *addSegmentListElement(
                                 struct segmentListElement **currentSegment,
                                 struct nodeListElement *sourceNode,
                                 struct nodeListElement *targetNode,
                                 struct stateInfo *state) {

    struct segmentListElement *newElement = NULL;

    /*
     * This skeleton modifying function does not lock the skeleton and therefore
     * has to be called from functions that do lock and NEVER directly.
     *
     */

    newElement = malloc(sizeof(struct segmentListElement));
    if(newElement == NULL) {
        LOG("Out of memory while trying to allocate memory for a new segmentListElement.");
        return NULL;
    }
    memset(newElement, '\0', sizeof(struct segmentListElement));
    if(*currentSegment == NULL) {
        // Requested to start a new list
        *currentSegment = newElement;
        newElement->previous = NULL;
        newElement->next = NULL;
    }
    else {
        newElement->previous = *currentSegment;
        newElement->next = (*currentSegment)->next;
        (*currentSegment)->next = newElement;
        if(newElement->next)
            newElement->next->previous = newElement;
    }

    //Valid segment
    newElement->flag = SEGMENT_FORWARD;

    newElement->source = sourceNode;
    newElement->target = targetNode;

    return newElement;
}

uint32_t UI_addSkeletonNode(Coordinate *clickedCoordinate, Byte VPtype, struct stateInfo *state) {
    int32_t addedNodeID;

    color4F treeCol;
    /* -1 causes new color assignment */
    treeCol.r = -1.;
    treeCol.g = -1.;
    treeCol.b = -1.;
    treeCol.a = 1.;

    if(!state->skeletonState->activeTree)
        addTreeListElement(TRUE, CHANGE_MANUAL, 0, treeCol);

    addedNodeID = addNode(CHANGE_MANUAL,
                          0,
                          state->skeletonState->defaultNodeRadius,
                          state->skeletonState->activeTree->treeID,
                          clickedCoordinate,
                          VPtype,
                          state->magnification,
                          -1,
                          TRUE,
                          state);
    if(!addedNodeID) {
        LOG("Error: Could not add new node!");
        return FALSE;
    }

    setActiveNode(CHANGE_MANUAL, NULL, addedNodeID);

    if((PTRSIZEINT)getDynArray(state->skeletonState->nodeCounter,
                               state->skeletonState->activeTree->treeID) == 1) {
        /* First node in this tree */

        pushBranchNode(CHANGE_MANUAL, TRUE, TRUE, NULL, addedNodeID, state);
        addComment(CHANGE_MANUAL, "First Node", NULL, addedNodeID, state);
    }

    return TRUE;
}

uint32_t UI_addSkeletonNodeAndLinkWithActive(Coordinate *clickedCoordinate, Byte VPtype, int32_t makeNodeActive) {
    int32_t targetNodeID;

    if(!state->skeletonState->activeNode) {
        LOG("Please create a node before trying to link nodes.");
        tempConfig->skeletonState->workMode = SKELETONIZER_ON_CLICK_ADD_NODE;
        return FALSE;
    }

    //Add a new node at the target position first.
    targetNodeID = addNode(CHANGE_MANUAL,
                           0,
                           state->skeletonState->defaultNodeRadius,
                           state->skeletonState->activeTree->treeID,
                           clickedCoordinate,
                           VPtype,
                           state->magnification,
                           -1,
                           TRUE,
                           state);
    if(!targetNodeID) {
        LOG("Could not add new node while trying to add node and link with active node!");
        return FALSE;
    }

    addSegment(CHANGE_MANUAL, state->skeletonState->activeNode->nodeID, targetNodeID, state);

    if(makeNodeActive)
        setActiveNode(CHANGE_MANUAL, NULL, targetNodeID);

    if((PTRSIZEINT)getDynArray(state->skeletonState->nodeCounter,
                               state->skeletonState->activeTree->treeID) == 1) {
        /* First node in this tree */

        pushBranchNode(CHANGE_MANUAL, TRUE, TRUE, NULL, targetNodeID, state);
        addComment(CHANGE_MANUAL, "First Node", NULL, targetNodeID, state);
    }

    return targetNodeID;
}

uint32_t setActiveNode(int32_t targetRevision,
                       struct nodeListElement *node,
                       int32_t nodeID) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    /*
     * If both *node and nodeID are specified, nodeID wins.
     * If neither *node nor nodeID are specified
     * (node == NULL and nodeID == 0), the active node is
     * set to NULL.
     *
     */

    if(targetRevision != CHANGE_NOSYNC) {
        if(lockSkeleton(targetRevision, state) == FALSE) {
            unlockSkeleton(FALSE, state);
            return FALSE;
        }
    }

    if(nodeID != 0) {
        node = findNodeByNodeID(nodeID, state);
        if(!node) {
            LOG("No node with id %d available.", nodeID);
            unlockSkeleton(FALSE, state);
            return FALSE;
        }
    }

    if(node)
        nodeID = node->nodeID;

    state->skeletonState->activeNode = node;
    state->skeletonState->viewChanged = TRUE;
    state->skeletonState->skeletonChanged = TRUE;

    if(node)
        setActiveTreeByID(node->correspondingTree->treeID, state);
    else
        state->skeletonState->activeTree = NULL;

    if(node) {
        if(node->comment) {
            state->skeletonState->currentComment = node->comment;
            memset(state->skeletonState->commentBuffer, '\0', 10240);

            strncpy(state->skeletonState->commentBuffer,
                    state->skeletonState->currentComment->content,
                    strlen(state->skeletonState->currentComment->content));
        }
        else
            memset(state->skeletonState->commentBuffer, '\0', 10240);
    }

    state->skeletonState->unsavedChanges = TRUE;

    if(targetRevision != CHANGE_NOSYNC) {
        state->skeletonState->skeletonRevision++;

        if(targetRevision == CHANGE_MANUAL) {
            if(!syncMessage(state, "brd", KIKI_SETACTIVENODE, nodeID))
                skeletonSyncBroken(state);
        }
        else
            refreshViewports(state);

        unlockSkeleton(TRUE, state);
    }

    if(node)
        state->viewerState->ag->activeNodeID = node->nodeID;

    /*
     * Calling drawGUI() here leads to a crash during synchronizationn.
     * Why? TDItem
     *
     */
    // drawGUI(state);

    return TRUE;
}

uint32_t setActiveTreeByID(int32_t treeID, struct stateInfo *state) {
    struct treeListElement *currentTree;

    currentTree = findTreeByTreeID(treeID, state);
    if(!currentTree) {
        LOG("There exists no tree with ID %d!", treeID);
        return FALSE;
    }

    state->skeletonState->activeTree = currentTree;
    state->skeletonState->skeletonChanged = TRUE;

    state->viewerState->ag->activeTreeID = currentTree->treeID;

    return TRUE;
}

int32_t saveSkeleton() {
    struct treeListElement *currentTree = NULL;
    struct nodeListElement *currentNode = NULL;
    PTRSIZEINT currentBranchPointID;
    struct segmentListElement *currentSegment = NULL;
    struct commentListElement *currentComment = NULL;
    struct stack *reverseBranchStack = NULL, *tempReverseStack = NULL;
    int32_t r;
    xmlChar attrString[128];

    xmlDocPtr xmlDocument;
    xmlNodePtr thingsXMLNode, nodesXMLNode, edgesXMLNode, currentXMLNode,
               paramsXMLNode, branchesXMLNode, commentsXMLNode;

    memset(attrString, '\0', 128 * sizeof(xmlChar));

    /*
     *  This function should always be called through UI_saveSkeleton
     *  for proper error and file name display to the user.
     */

    /*
     *  We need to do this to be able to save the branch point stack to a file
     *  and still have the branch points available to the user afterwards.
     *
     */

    reverseBranchStack = newStack(2048);
    tempReverseStack = newStack(2048);
    while((currentBranchPointID =
          (PTRSIZEINT)popStack(state->skeletonState->branchStack))) {
        pushStack(reverseBranchStack, (void *)currentBranchPointID);
        pushStack(tempReverseStack, (void *)currentBranchPointID);
    }

    while((currentBranchPointID =
          (PTRSIZEINT)popStack(tempReverseStack))) {
        currentNode = (struct nodeListElement *)findNodeByNodeID(currentBranchPointID, state);
        pushBranchNode(CHANGE_MANUAL, FALSE, FALSE, currentNode, 0, state);
    }

    xmlDocument = xmlNewDoc(BAD_CAST"1.0");
    thingsXMLNode = xmlNewDocNode(xmlDocument, NULL, BAD_CAST"things", NULL);
    xmlDocSetRootElement(xmlDocument, thingsXMLNode);

    paramsXMLNode = xmlNewTextChild(thingsXMLNode, NULL, BAD_CAST"parameters", NULL);

    currentXMLNode = xmlNewTextChild(paramsXMLNode, NULL, BAD_CAST"experiment", NULL);
    xmlStrPrintf(attrString, 128, BAD_CAST"%s", state->name);
    xmlNewProp(currentXMLNode, BAD_CAST"name", attrString);
    memset(attrString, '\0', 128);

    currentXMLNode = xmlNewTextChild(paramsXMLNode, NULL, BAD_CAST"scale", NULL);
    xmlStrPrintf(attrString, 128, BAD_CAST"%f", state->scale.x / state->magnification);
    xmlNewProp(currentXMLNode, BAD_CAST"x", attrString);
    memset(attrString, '\0', 128);
    xmlStrPrintf(attrString, 128, BAD_CAST"%f", state->scale.y / state->magnification);
    xmlNewProp(currentXMLNode, BAD_CAST"y", attrString);
    memset(attrString, '\0', 128);
    xmlStrPrintf(attrString, 128, BAD_CAST"%f", state->scale.z / state->magnification);
    xmlNewProp(currentXMLNode, BAD_CAST"z", attrString);
    memset(attrString, '\0', 128);

    currentXMLNode = xmlNewTextChild(paramsXMLNode, NULL, BAD_CAST"offset", NULL);
    xmlStrPrintf(attrString, 128, BAD_CAST"%d", state->offset.x / state->magnification);
    xmlNewProp(currentXMLNode, BAD_CAST"x", attrString);
    memset(attrString, '\0', 128);
    xmlStrPrintf(attrString, 128, BAD_CAST"%d", state->offset.y / state->magnification);
    xmlNewProp(currentXMLNode, BAD_CAST"y", attrString);
    memset(attrString, '\0', 128);
    xmlStrPrintf(attrString, 128, BAD_CAST"%d", state->offset.z / state->magnification);
    xmlNewProp(currentXMLNode, BAD_CAST"z", attrString);
    memset(attrString, '\0', 128);

    currentXMLNode = xmlNewTextChild(paramsXMLNode, NULL, BAD_CAST"time", NULL);
    xmlStrPrintf(attrString,
                 128,
                 BAD_CAST"%d",
                 state->skeletonState->skeletonTime - state->skeletonState->skeletonTimeCorrection + SDL_GetTicks());
    xmlNewProp(currentXMLNode, BAD_CAST"ms", attrString);
    memset(attrString, '\0', 128);

    if(state->skeletonState->activeNode) {
        currentXMLNode = xmlNewTextChild(paramsXMLNode, NULL, BAD_CAST"activeNode", NULL);
        xmlStrPrintf(attrString,
                     128,
                     BAD_CAST"%d",
                     state->skeletonState->activeNode->nodeID);
        xmlNewProp(currentXMLNode, BAD_CAST"id", attrString);
        memset(attrString, '\0', 128);
    }

    currentXMLNode = xmlNewTextChild(paramsXMLNode, NULL, BAD_CAST"editPosition", NULL);
    xmlStrPrintf(attrString, 128, BAD_CAST"%d", state->viewerState->currentPosition.x + 1);
    xmlNewProp(currentXMLNode, BAD_CAST"x", attrString);
    memset(attrString, '\0', 128);
    xmlStrPrintf(attrString, 128, BAD_CAST"%d", state->viewerState->currentPosition.y + 1);
    xmlNewProp(currentXMLNode, BAD_CAST"y", attrString);
    memset(attrString, '\0', 128);
    xmlStrPrintf(attrString, 128, BAD_CAST"%d", state->viewerState->currentPosition.z + 1);
    xmlNewProp(currentXMLNode, BAD_CAST"z", attrString);
    memset(attrString, '\0', 128);

    currentTree = state->skeletonState->firstTree;
    if((currentTree == NULL) && (state->skeletonState->currentComment == NULL)) {
        return 0; //No Skeleton to save
    }

    while(currentTree) {
        /*
         * Every "thing" has associated nodes and edges.
         *
         */
        currentXMLNode = xmlNewTextChild(thingsXMLNode, NULL, BAD_CAST"thing", NULL);
        xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentTree->treeID);
        xmlNewProp(currentXMLNode, BAD_CAST"id", attrString);
        memset(attrString, '\0', 128);

        xmlStrPrintf(attrString, 128, BAD_CAST"%f", currentTree->color.r);
        xmlNewProp(currentXMLNode, BAD_CAST"color.r", attrString);
        memset(attrString, '\0', 128);

        xmlStrPrintf(attrString, 128, BAD_CAST"%f", currentTree->color.g);
        xmlNewProp(currentXMLNode, BAD_CAST"color.g", attrString);
        memset(attrString, '\0', 128);

        xmlStrPrintf(attrString, 128, BAD_CAST"%f", currentTree->color.b);
        xmlNewProp(currentXMLNode, BAD_CAST"color.b", attrString);
        memset(attrString, '\0', 128);

        xmlStrPrintf(attrString, 128, BAD_CAST"%f", currentTree->color.a);
        xmlNewProp(currentXMLNode, BAD_CAST"color.a", attrString);
        memset(attrString, '\0', 128);

        nodesXMLNode = xmlNewTextChild(currentXMLNode, NULL, BAD_CAST"nodes", NULL);
        edgesXMLNode = xmlNewTextChild(currentXMLNode, NULL, BAD_CAST"edges", NULL);

        currentNode = currentTree->firstNode;
        while(currentNode) {
            currentXMLNode = xmlNewTextChild(nodesXMLNode, NULL, BAD_CAST"node", NULL);

            xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentNode->nodeID);
            xmlNewProp(currentXMLNode, BAD_CAST"id", attrString);
            memset(attrString, '\0', 128);

            xmlStrPrintf(attrString, 128, BAD_CAST"%f", currentNode->radius);
            xmlNewProp(currentXMLNode, BAD_CAST"radius", attrString);
            memset(attrString, '\0', 128);

            xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentNode->position.x + 1);
            xmlNewProp(currentXMLNode, BAD_CAST"x", attrString);
            memset(attrString, '\0', 128);

            xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentNode->position.y + 1);
            xmlNewProp(currentXMLNode, BAD_CAST"y", attrString);
            memset(attrString, '\0', 128);

            xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentNode->position.z + 1);
            xmlNewProp(currentXMLNode, BAD_CAST"z", attrString);
            memset(attrString, '\0', 128);

            xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentNode->createdInVp);
            xmlNewProp(currentXMLNode, BAD_CAST"inVp", attrString);
            memset(attrString, '\0', 128);

            xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentNode->createdInMag);
            xmlNewProp(currentXMLNode, BAD_CAST"inMag", attrString);
            memset(attrString, '\0', 128);

            xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentNode->timestamp);
            xmlNewProp(currentXMLNode, BAD_CAST"time", attrString);
            memset(attrString, '\0', 128);

            currentSegment = currentNode->firstSegment;
            while(currentSegment) {
                if(currentSegment->flag == SEGMENT_FORWARD) {
                    currentXMLNode = xmlNewTextChild(edgesXMLNode, NULL, BAD_CAST"edge", NULL);

                    xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentSegment->source->nodeID);
                    xmlNewProp(currentXMLNode, BAD_CAST"source", attrString);
                    memset(attrString, '\0', 128);

                    xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentSegment->target->nodeID);
                    xmlNewProp(currentXMLNode, BAD_CAST"target", attrString);
                    memset(attrString, '\0', 128);
                }

                currentSegment = currentSegment->next;
            }

            currentNode = currentNode->next;
        }

        currentTree = currentTree->next;
    }


    commentsXMLNode = xmlNewTextChild(thingsXMLNode, NULL, BAD_CAST"comments", NULL);
    currentComment = state->skeletonState->currentComment;
    if(state->skeletonState->currentComment != NULL) {
        do {
            currentXMLNode = xmlNewTextChild(commentsXMLNode, NULL, BAD_CAST"comment", NULL);
            xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentComment->node->nodeID);
            xmlNewProp(currentXMLNode, BAD_CAST"node", attrString);
            memset(attrString, '\0', 128);
            xmlNewProp(currentXMLNode, BAD_CAST"content", BAD_CAST currentComment->content);
            currentComment = currentComment->next;
        } while (currentComment != state->skeletonState->currentComment);
    }


    branchesXMLNode = xmlNewTextChild(thingsXMLNode, NULL, BAD_CAST"branchpoints", NULL);
    while((currentBranchPointID = (PTRSIZEINT)popStack(reverseBranchStack))) {
        currentXMLNode = xmlNewTextChild(branchesXMLNode,
                                         NULL,
                                         BAD_CAST"branchpoint",
                                         NULL);

#ifdef ARCH_64
        xmlStrPrintf(attrString, 128, BAD_CAST"%"PRId64, currentBranchPointID);
#else
        xmlStrPrintf(attrString, 128, BAD_CAST"%d", currentBranchPointID);
#endif

        xmlNewProp(currentXMLNode, BAD_CAST"id", attrString);
        memset(attrString, '\0', 128);
    }

    r = xmlSaveFormatFile(state->skeletonState->skeletonFile, xmlDocument, 1);

    xmlFreeDoc(xmlDocument);
    return r;
}

uint32_t updateSkeletonFileName(int32_t targetRevision, int32_t increment, char *filename) {
    int32_t extensionDelim = -1, countDelim = -1;
    char count[8192];
    char origFilename[8192], skeletonFileBase[8192];
    int32_t i;

    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    memset(skeletonFileBase, '\0', 8192);
    memset(origFilename, '\0', 8192);
    strncpy(origFilename, filename, 8192 - 1);

    if(increment) {
        for(i = 8192 - 1; i >= 0; i--) {
            if(filename[i] == '.') {
                extensionDelim = i;
                break;
            }
        }

        for(i--; i >= 0; i--) {
            if(filename[i] == '.') {
                countDelim = i;
                break;
            }
        }

        if(countDelim > -1) {
            strncpy(skeletonFileBase,
                    filename,
                    countDelim);
        }
        else if(extensionDelim > -1){
            strncpy(skeletonFileBase,
                    filename,
                    extensionDelim);
        }
        else {
            strncpy(skeletonFileBase,
                    filename,
                    8192 - 1);
        }

        if(countDelim && extensionDelim) {
            strncpy(count, &filename[countDelim + 1], extensionDelim - countDelim);
            state->skeletonState->saveCnt = atoi(count);
            state->skeletonState->saveCnt++;
        }
        else {
            state->skeletonState->saveCnt = 0;
        }

        sprintf(state->skeletonState->skeletonFile, "%s.%.3d.nml",
                skeletonFileBase,
                state->skeletonState->saveCnt);
    }

    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "blrds", KIKI_SKELETONFILENAME, increment, origFilename))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

uint32_t loadSkeleton() {
    xmlDocPtr xmlDocument;
    xmlNodePtr currentXMLNode, thingsXMLNode, thingOrParamXMLNode, nodesEdgesXMLNode;
    int32_t neuronID = 0, nodeID = 0, merge = FALSE;
    int32_t nodeID1, nodeID2, greatestNodeIDbeforeLoading, greatestTreeIDbeforeLoading;
    float radius;
    Byte VPtype;
    int32_t inMag, magnification = 0;
    int32_t globalMagnificationSpecified = FALSE;
    xmlChar *attribute;
    struct treeListElement *currentTree;
    struct nodeListElement *currentNode;
    Coordinate *currentCoordinate, loadedPosition;
    Coordinate offset;
    floatCoordinate scale;
    int32_t time, activeNodeID = 0;
    int32_t skeletonTime = 0;
    color4F neuronColor;

    LOG("Starting to load skeleton...");

    /*
     *  This function should always be called through UI_loadSkeleton for
     *  proper error and file name display to the user.
     */

    /*
     *  There's no sanity check for values read from files, yet.
     */

    /*
     *  These defaults should usually always be overridden by values
     *  given in the file.
     */

    SET_COORDINATE(offset, state->offset.x, state->offset.y, state->offset.z);
    SET_COORDINATE(scale, state->scale.x, state->scale.y, state->scale.z);
    SET_COORDINATE(loadedPosition, 0, 0, 0);

    currentCoordinate = malloc(sizeof(Coordinate));
    if(currentCoordinate == NULL) {
        LOG("Out of memory.");
        return FALSE;
    }
    memset(currentCoordinate, '\0', sizeof(currentCoordinate));

    if(!state->skeletonState->mergeOnLoadFlag) {
        merge = FALSE;
        clearSkeleton(CHANGE_MANUAL);
    }
    else {
        merge = TRUE;
        greatestNodeIDbeforeLoading = state->skeletonState->greatestNodeID;
        greatestTreeIDbeforeLoading = state->skeletonState->greatestTreeID;
    }

    xmlDocument = xmlParseFile(state->skeletonState->skeletonFile);



    if(xmlDocument == NULL) {
        LOG("Document not parsed successfully.");
        return FALSE;
    }

    thingsXMLNode = xmlDocGetRootElement(xmlDocument);
    if(thingsXMLNode == NULL) {
        LOG("Empty document.");
        xmlFreeDoc(xmlDocument);
        return FALSE;
    }

    if(xmlStrEqual(thingsXMLNode->name, (const xmlChar *)"things") == FALSE) {
        LOG("Root element %s in file %s unrecognized. Not loading.",
            thingsXMLNode->name,
            state->skeletonState->skeletonFile);
        return FALSE;
    }

    thingOrParamXMLNode = thingsXMLNode->xmlChildrenNode;
    while(thingOrParamXMLNode) {
        if(xmlStrEqual(thingOrParamXMLNode->name, (const xmlChar *)"parameters")) {
            currentXMLNode = thingOrParamXMLNode->children;
            while(currentXMLNode) {
                if(xmlStrEqual(currentXMLNode->name, (const xmlChar *)"magnification")) {
                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"factor");
                    /*
                     * This is for legacy skeleton files.
                     * In the past, magnification was specified on a per-file basis
                     * or not specified at all.
                     * A magnification factor of 0 shall represent an unknown magnification.
                     *
                     */
                    if(attribute) {
                        magnification = atoi((char *)attribute);
                        globalMagnificationSpecified = TRUE;
                    }
                    else
                        magnification = 0;
                }
                if(xmlStrEqual(currentXMLNode->name, (const xmlChar *)"offset")) {
                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"x");
                    if(attribute)
                        offset.x = atoi((char *)attribute);

                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"y");
                    if(attribute)
                        offset.y = atoi((char *)attribute);

                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"z");
                    if(attribute)
                        offset.z = atoi((char *)attribute);
                }

                if(xmlStrEqual(currentXMLNode->name, (const xmlChar *)"time")) {
                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"ms");
                    if(attribute)
                        skeletonTime = atoi((char *)attribute);
                }

                if(xmlStrEqual(currentXMLNode->name, (const xmlChar *)"activeNode")) {
                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"id");
                    if(attribute)
                        activeNodeID = atoi((char *)attribute);
                }

                if(xmlStrEqual(currentXMLNode->name, (const xmlChar *)"scale")) {
                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"x");
                    if(attribute)
                        scale.x = atof((char *)attribute);

                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"y");
                    if(attribute)
                        scale.y = atof((char *)attribute);

                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"z");
                    if(attribute)
                        scale.z = atof((char *)attribute);
                }

                if(xmlStrEqual(currentXMLNode->name, (const xmlChar *)"editPosition")) {
                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"x");
                    if(attribute)
                        loadedPosition.x = atoi((char *)attribute);

                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"y");
                    if(attribute)
                        loadedPosition.y = atoi((char *)attribute);

                    attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"z");
                    if(attribute)
                        loadedPosition.z = atoi((char *)attribute);
                }

                currentXMLNode = currentXMLNode->next;
            }
        }

        if(xmlStrEqual(thingOrParamXMLNode->name,
                       (const xmlChar *)"comments")) {

            currentXMLNode = thingOrParamXMLNode->children;
            while(currentXMLNode) {
                if(xmlStrEqual(currentXMLNode->name,
                               (const xmlChar *)"comment")) {

                    attribute = xmlGetProp(currentXMLNode,
                                           (const xmlChar *)"node");
                    if(attribute) {
                        if(!merge)
                            nodeID = atoi((char *)attribute);
                        else
                            nodeID = atoi((char *)attribute) + greatestNodeIDbeforeLoading;

                        currentNode = findNodeByNodeID(nodeID, state);
                    }
                    attribute = xmlGetProp(currentXMLNode,
                                           (const xmlChar *)"content");
                    if(attribute && currentNode) {
                        addComment(CHANGE_MANUAL, (char *)attribute, currentNode, 0, state);
                    }
                }

                currentXMLNode = currentXMLNode->next;
            }
        }


        if(xmlStrEqual(thingOrParamXMLNode->name,
                       (const xmlChar *)"branchpoints")) {

            currentXMLNode = thingOrParamXMLNode->children;
            while(currentXMLNode) {
                if(xmlStrEqual(currentXMLNode->name,
                               (const xmlChar *)"branchpoint")) {

                    attribute = xmlGetProp(currentXMLNode,
                                           (const xmlChar *)"id");
                    if(attribute) {
                        if(!merge)
                            nodeID = atoi((char *)attribute);
                        else
                            nodeID = atoi((char *)attribute) + greatestNodeIDbeforeLoading;

                        currentNode = findNodeByNodeID(nodeID, state);
                        if(currentNode)
                            pushBranchNode(CHANGE_MANUAL, TRUE, FALSE, currentNode, 0, state);
                    }
                }

                currentXMLNode = currentXMLNode->next;
            }
        }

        if(xmlStrEqual(thingOrParamXMLNode->name, (const xmlChar *)"thing")) {
            /* Add tree */
            attribute = xmlGetProp(thingOrParamXMLNode, (const xmlChar *) "id");
            if(attribute) {
                neuronID = atoi((char *)attribute);
                free(attribute);
            }
            else
                neuronID = 0;   /* Whatever. */

            /* -1. causes default color assignment based on ID*/
            attribute = xmlGetProp(thingOrParamXMLNode, (const xmlChar *) "color.r");
            if(attribute) {
                neuronColor.r = (float)strtod((char *)attribute, (char **)NULL);
                free(attribute);
            }
            else
                neuronColor.r = -1.;

            attribute = xmlGetProp(thingOrParamXMLNode, (const xmlChar *) "color.g");
            if(attribute) {
                neuronColor.g = (float)strtod((char *)attribute, (char **)NULL);
                free(attribute);
            }
            else
                neuronColor.g = -1.;

            attribute = xmlGetProp(thingOrParamXMLNode, (const xmlChar *) "color.b");
            if(attribute) {
                neuronColor.b = (float)strtod((char *)attribute, (char **)NULL);
                free(attribute);
            }
            else
                neuronColor.b = -1.;

            attribute = xmlGetProp(thingOrParamXMLNode, (const xmlChar *) "color.a");
            if(attribute) {
                neuronColor.a = (float)strtod((char *)attribute, (char **)NULL);
                free(attribute);
            }
            else
                neuronColor.a = 1.;

           if(!merge) {
                addTreeListElement(TRUE, CHANGE_MANUAL, neuronID, neuronColor);
                setActiveTreeByID(neuronID, state);
            }
            else {
                neuronID += greatestTreeIDbeforeLoading;
                currentTree = addTreeListElement(TRUE, CHANGE_MANUAL, neuronID, neuronColor);
                setActiveTreeByID(currentTree->treeID, state);
                neuronID = currentTree->treeID;
            }

            nodesEdgesXMLNode = thingOrParamXMLNode->children;
            while(nodesEdgesXMLNode) {
                if(xmlStrEqual(nodesEdgesXMLNode->name, (const xmlChar *)"nodes")) {
                    currentXMLNode = nodesEdgesXMLNode->children;
                    while(currentXMLNode) {
                        if(xmlStrEqual(currentXMLNode->name, (const xmlChar *)"node")) {
                            /* Add node. */

                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"id");
                            if(attribute) {
                                nodeID = atoi((char *)attribute);
                                free(attribute);
                            }
                            else
                                nodeID = 0;

                            LOG("Adding node: %d", nodeID);
                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"radius");
                            if(attribute) {
                                radius = (float)strtod((char *)attribute, (char **)NULL);
                                free(attribute);
                            }
                            else
                                radius = state->skeletonState->defaultNodeRadius;

                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"x");
                            if(attribute) {
                                currentCoordinate->x = atoi((char *)attribute) - 1;
                                if(globalMagnificationSpecified)
                                    currentCoordinate->x = currentCoordinate->x * magnification;
                                free(attribute);
                            }
                            else
                                currentCoordinate->x = 0;

                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"y");
                            if(attribute) {
                                currentCoordinate->y = atoi((char *)attribute) - 1;
                                if(globalMagnificationSpecified)
                                    currentCoordinate->y = currentCoordinate->y * magnification;
                                free(attribute);
                            }
                            else
                                currentCoordinate->y = 0;

                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"z");
                            if(attribute) {
                                currentCoordinate->z = atoi((char *)attribute) - 1;
                                if(globalMagnificationSpecified)
                                    currentCoordinate->z = currentCoordinate->z * magnification;
                                free(attribute);
                            }
                            else
                                currentCoordinate->z = 0;

                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"inVp");
                            if(attribute) {
                                VPtype = atoi((char *)attribute);
                                free(attribute);
                            }
                            else
                                VPtype = VIEWPORT_UNDEFINED;

                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"inMag");
                            if(attribute) {
                                inMag = atoi((char *)attribute);
                                free(attribute);
                            }
                            else
                                inMag = magnification; /* For legacy skeleton files */

                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"time");
                            if(attribute) {
                                time = atoi((char *)attribute);
                                free(attribute);
                            }
                            else
                                time = skeletonTime; /* For legacy skeleton files */

                            if(!merge)
                                addNode(CHANGE_MANUAL, nodeID, radius, neuronID, currentCoordinate, VPtype, inMag, time, FALSE, state);
                            else {
                                nodeID += greatestNodeIDbeforeLoading;
                                addNode(CHANGE_MANUAL, nodeID, radius, neuronID, currentCoordinate, VPtype, inMag, time, FALSE,  state);
                            }
                        }

                        currentXMLNode = currentXMLNode->next;
                    }
                }
                else if(xmlStrEqual(nodesEdgesXMLNode->name, (const xmlChar *)"edges")) {
                    currentXMLNode = nodesEdgesXMLNode->children;
                    while(currentXMLNode) {
                        if(xmlStrEqual(currentXMLNode->name, (const xmlChar *)"edge")) {
                            /* Add edge */
                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"source");
                            if(attribute) {
                                nodeID1 = atoi((char *)attribute);
                                free(attribute);
                            }
                            else
                                nodeID1 = 0;

                            attribute = xmlGetProp(currentXMLNode, (const xmlChar *)"target");
                            if(attribute) {
                                nodeID2 = atoi((char *)attribute);
                                free(attribute);
                            }
                            else
                                nodeID2 = 0;

                            // printf("Trying to add a segment between %d and %d\n", nodeID1, nodeID2);
                            if(!merge)
                                addSegment(CHANGE_MANUAL, nodeID1, nodeID2, state);
                            else
                                addSegment(CHANGE_MANUAL, nodeID1 + greatestNodeIDbeforeLoading, nodeID2 + greatestNodeIDbeforeLoading, state);

                        }
                        currentXMLNode = currentXMLNode->next;
                    }
                }

                nodesEdgesXMLNode = nodesEdgesXMLNode->next;
            }
        }

        thingOrParamXMLNode = thingOrParamXMLNode->next;
    }

    free(currentCoordinate);
    xmlFreeDoc(xmlDocument);

    addRecentFile(state->skeletonState->skeletonFile, FALSE);

    if(activeNodeID) {
        setActiveNode(CHANGE_MANUAL, NULL, activeNodeID);
    }

    if((loadedPosition.x != 0) &&
       (loadedPosition.y != 0) &&
       (loadedPosition.z != 0)) {
        tempConfig->viewerState->currentPosition.x =
            loadedPosition.x - 1;
        tempConfig->viewerState->currentPosition.y =
            loadedPosition.y - 1;
        tempConfig->viewerState->currentPosition.z =
            loadedPosition.z - 1;

        updatePosition(state, TELL_COORDINATE_CHANGE);
    }

    tempConfig->skeletonState->workMode = SKELETONIZER_ON_CLICK_ADD_NODE;
    state->skeletonState->skeletonTime = skeletonTime;
    state->skeletonState->skeletonTimeCorrection = SDL_GetTicks();

    return TRUE;
}

uint32_t delActiveNode(struct stateInfo *state) {
    if(state->skeletonState->activeNode) {
        delNode(CHANGE_MANUAL, 0, state->skeletonState->activeNode, state);
    }
    else {
        return FALSE;
    }

    return TRUE;
}

struct nodeListElement *findNearbyNode(struct treeListElement *nearbyTree,
                                       Coordinate searchPosition,
                                       struct stateInfo *state) {

    struct nodeListElement *currentNode = NULL;
    struct nodeListElement *nodeWithCurrentlySmallestDistance = NULL;
    struct treeListElement *currentTree = NULL;
    floatCoordinate distanceVector;
    float smallestDistance = 10000000000.; //:-(

    /*
     *  If available, search for a node within nearbyTree first.
     */

    if(nearbyTree) {
        currentNode = nearbyTree->firstNode;
        while(currentNode) {
            // We make the nearest node the next active node
            distanceVector.x = (float)searchPosition.x - (float)currentNode->position.x;
            distanceVector.y = (float)searchPosition.y - (float)currentNode->position.y;
            distanceVector.z = (float)searchPosition.z - (float)currentNode->position.z;

            if(euclidicNorm(&distanceVector) < smallestDistance) {
                smallestDistance = euclidicNorm(&distanceVector);
                nodeWithCurrentlySmallestDistance = currentNode;
            }
            currentNode = currentNode->next;
        }

        if(nodeWithCurrentlySmallestDistance)
            return nodeWithCurrentlySmallestDistance;
    }

    /*
     * Ok, we didn't find any node in nearbyTree.
     * Now we take the nearest node, independent of the tree it belongs to.
     */

    currentTree = state->skeletonState->firstTree;
    while(currentTree) {
        currentNode = currentTree->firstNode;

        while(currentNode) {
            //We make the nearest node the next active node
            distanceVector.x = (float)searchPosition.x - (float)currentNode->position.x;
            distanceVector.y = (float)searchPosition.y - (float)currentNode->position.y;
            distanceVector.z = (float)searchPosition.z - (float)currentNode->position.z;

            if(euclidicNorm(&distanceVector) < smallestDistance) {
                smallestDistance = euclidicNorm(&distanceVector);
                nodeWithCurrentlySmallestDistance = currentNode;
            }

            currentNode = currentNode->next;
        }

       currentTree = currentTree->next;
    }

    if(nodeWithCurrentlySmallestDistance)
        return nodeWithCurrentlySmallestDistance;
    else
        return NULL;
}

uint32_t delActiveTree(struct stateInfo *state) {
    if(state->skeletonState->activeTree)
        delTree(CHANGE_MANUAL, state->skeletonState->activeTree->treeID, state);
    else {
       LOG("No active tree available.");
       return FALSE;
    }

    tempConfig->skeletonState->workMode = SKELETONIZER_ON_CLICK_ADD_NODE;

    return TRUE;
}

/*
 * We have to delete the node out of 2 structures: the skeleton nested linked list structure
 * and the skeleton visualization structure (hashtable with skeletonDCs).
 */
uint32_t delNode(int32_t targetRevision, int32_t nodeID, struct nodeListElement *nodeToDel, struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    struct segmentListElement *currentSegment;
    struct segmentListElement *tempNext = NULL;
    struct nodeListElement *newActiveNode = NULL;
    int32_t treeID;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    if(!nodeToDel)
        nodeToDel = findNodeByNodeID(nodeID, state);

    nodeID = nodeToDel->nodeID;

    if(!nodeToDel) {
        LOG("The given node %d doesn't exist. Unable to delete it.", nodeID);
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    treeID = nodeToDel->correspondingTree->treeID;

    if(nodeToDel->comment)
        delComment(CHANGE_MANUAL, nodeToDel->comment, 0, state);

    /*
     * First, delete all segments pointing towards and away of the nodeToDelhas
     * been */

    currentSegment = nodeToDel->firstSegment;

    while(currentSegment) {

        tempNext = currentSegment->next;

        if(currentSegment->flag == SEGMENT_FORWARD)
            delSegment(CHANGE_MANUAL, 0,0, currentSegment, state);
        else if(currentSegment->flag == SEGMENT_BACKWARD)
            delSegment(CHANGE_MANUAL, 0,0, currentSegment->reverseSegment, state);

        currentSegment = tempNext;
    }

    nodeToDel->firstSegment = NULL;

    /*
     * Delete the node out of the visualization structure
     */

    delNodeFromSkeletonStruct(nodeToDel, state);

    if(nodeToDel == nodeToDel->correspondingTree->firstNode)
        nodeToDel->correspondingTree->firstNode = nodeToDel->next;
    else {
        if(nodeToDel->previous)
            nodeToDel->previous->next = nodeToDel->next;
        if(nodeToDel->next)
            nodeToDel->next->previous = nodeToDel->previous;
    }

    setDynArray(state->skeletonState->nodesByNodeID, nodeToDel->nodeID, NULL);

    if(state->skeletonState->activeNode == nodeToDel) {
        newActiveNode = findNearbyNode(nodeToDel->correspondingTree,
                                       nodeToDel->position,
                                       state);

        setActiveNode(CHANGE_NOSYNC, newActiveNode, 0);
    }

    free(nodeToDel);

    state->skeletonState->totalNodeElements--;
    state->viewerState->ag->totalNodes--;

    setDynArray(state->skeletonState->nodeCounter,
            treeID,
            (void *)((PTRSIZEINT)getDynArray(state->skeletonState->nodeCounter, treeID) - 1));

    state->skeletonState->skeletonChanged = TRUE;
    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brd", KIKI_DELNODE, nodeID))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

uint32_t delTree(int32_t targetRevision, int32_t treeID, struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    struct treeListElement *currentTree;
    struct nodeListElement *currentNode, *nodeToDel;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    currentTree = findTreeByTreeID(treeID, state);
    if(!currentTree) {
        LOG("There exists no tree with ID %d. Unable to delete it.", treeID);
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    currentNode = currentTree->firstNode;
    while(currentNode) {
        nodeToDel = currentNode;
        currentNode = nodeToDel->next;
        delNode(CHANGE_MANUAL, 0, nodeToDel, state);
    }
    currentTree->firstNode = NULL;

    if(currentTree == state->skeletonState->firstTree)
        state->skeletonState->firstTree = currentTree->next;
    else {
        if(currentTree->previous)
            currentTree->previous->next = currentTree->next;
        if(currentTree->next)
            currentTree->next->previous = currentTree->previous;
    }

    free(currentTree);

    state->skeletonState->treeElements--;

    state->skeletonState->skeletonChanged = TRUE;
    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brd", KIKI_DELTREE, treeID))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

struct nodeListElement *findNodeByNodeID(int32_t nodeID, struct stateInfo *state) {
    struct nodeListElement *node;

    node = (struct nodeListElement *)getDynArray(state->skeletonState->nodesByNodeID, nodeID);

    return node;
}

struct nodeListElement *findNodeByCoordinate(Coordinate *position, struct stateInfo *state) {
    struct nodeListElement *currentNode;
    struct treeListElement *currentTree;

    currentTree = state->skeletonState->firstTree;

    while(currentTree) {
        currentNode = currentTree->firstNode;
        while(currentNode) {
            if(COMPARE_COORDINATE(currentNode->position, *position))
                return currentNode;

            currentNode = currentNode->next;
        }
        currentTree = currentTree->next;
    }
    return NULL;
}

struct segmentListElement *findSegmentByNodeIDs(int32_t sourceNodeID, int32_t targetNodeID, struct stateInfo *state) {
    struct segmentListElement *currentSegment;
    struct nodeListElement *currentNode;

    currentNode = findNodeByNodeID(sourceNodeID, state);

    if(!currentNode) return NULL;

    currentSegment = currentNode->firstSegment;
    while(currentSegment) {
        if(currentSegment->flag == SEGMENT_BACKWARD) {
            currentSegment = currentSegment->next;
            continue;
        }
        if(currentSegment->target->nodeID == targetNodeID)
            return currentSegment;
        currentSegment = currentSegment->next;
    }

    return NULL;
}

struct treeListElement *findTreeByTreeID(int32_t treeID, struct stateInfo *state) {
    struct treeListElement *currentTree;

    currentTree = state->skeletonState->firstTree;

    while(currentTree) {
        if(currentTree->treeID == treeID)
            return currentTree;
        currentTree = currentTree->next;
    }

    return NULL;
}

uint32_t clearSkeleton(int32_t targetRevision) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    struct treeListElement *currentTree, *treeToDel;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    currentTree = state->skeletonState->firstTree;
    while(currentTree) {
        treeToDel = currentTree;
        currentTree = treeToDel->next;
        delTree(CHANGE_MANUAL, treeToDel->treeID, state);
    }

    state->skeletonState->activeNode = NULL;
    state->skeletonState->activeTree = NULL;

    state->skeletonState->skeletonTime = 0;
    state->skeletonState->skeletonTimeCorrection = SDL_GetTicks();

    ht_rmtable(state->skeletonState->skeletonDCs);
    delDynArray(state->skeletonState->nodeCounter);
    delDynArray(state->skeletonState->nodesByNodeID);
    delStack(state->skeletonState->branchStack);

    //Create a new hash-table that holds the skeleton datacubes
    state->skeletonState->skeletonDCs = ht_new(state->skeletonState->skeletonDCnumber);
    if(state->skeletonState->skeletonDCs == HT_FAILURE) {
        LOG("Unable to create skeleton hash-table.");
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    //Generate empty tree structures
    state->skeletonState->firstTree = NULL;
    state->skeletonState->totalNodeElements = 0;
    state->skeletonState->totalSegmentElements = 0;
    state->skeletonState->treeElements = 0;
    state->skeletonState->activeTree = NULL;
    state->skeletonState->activeNode = NULL;

    state->skeletonState->nodeCounter = newDynArray(128);
    state->skeletonState->nodesByNodeID = newDynArray(1048576);
    state->skeletonState->branchStack = newStack(2048);

    tempConfig->skeletonState->workMode = SKELETONIZER_ON_CLICK_ADD_NODE;

    state->skeletonState->skeletonRevision++;
    state->skeletonState->unsavedChanges = TRUE;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "br", KIKI_CLEARSKELETON))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    state->skeletonState->skeletonRevision = 0;

    unlockSkeleton(TRUE, state);

    return TRUE;
}

uint32_t genTestNodes(uint32_t number, struct stateInfo *state) {
    uint32_t i;
    Coordinate pos;
    srand( time(NULL) );

    for(i = 1; i < number; i++) {
        pos.x = (int32_t)(((double)rand() / (double)RAND_MAX) * (double)state->boundary.x);
        pos.y = (int32_t)(((double)rand() / (double)RAND_MAX) * (double)state->boundary.y);
        pos.z = (int32_t)(((double)rand() / (double)RAND_MAX) * (double)state->boundary.z);
        addNode(CHANGE_MANUAL, 0, state->skeletonState->defaultNodeRadius, 1, &pos, VIEWPORT_UNDEFINED, state->magnification, 0, FALSE, state);
    }

    return TRUE;
}


uint32_t mergeTrees(int32_t targetRevision, int32_t treeID1, int32_t treeID2, struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    struct treeListElement *tree1, *tree2;
    struct nodeListElement *currentNode;
    struct nodeListElement *firstNode, *lastNode;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    tree1 = findTreeByTreeID(treeID1, state);
    tree2 = findTreeByTreeID(treeID2, state);

    if(!(tree1) || !(tree2)) {
        LOG("Could not merge trees, provided IDs are not valid!");
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    currentNode = tree2->firstNode;

    while(currentNode) {
        //Change the corresponding tree
        currentNode->correspondingTree = tree1;

        currentNode = currentNode->next;
    }

    //Now we insert the node list of tree2 before the node list of tree1
    if(tree1->firstNode && tree2->firstNode) {
        //First, we have to find the last node of tree2 (this node has to be connected
        //to the first node inside of tree1)
        firstNode = tree2->firstNode;
        lastNode = firstNode;
        while(lastNode->next)
            lastNode = lastNode->next;

        tree1->firstNode->previous = lastNode;
        lastNode->next = tree1->firstNode;
        tree1->firstNode = firstNode;

    }
    else if(tree2->firstNode)
        tree1->firstNode = tree2->firstNode;

    // The new node count for tree1 is the old node count of tree1 plus the node
    // count of tree2
    setDynArray(state->skeletonState->nodeCounter,
                tree1->treeID,
                (void *)
                ((PTRSIZEINT)getDynArray(state->skeletonState->nodeCounter,
                                        tree1->treeID) +
                (PTRSIZEINT)getDynArray(state->skeletonState->nodeCounter,
                                        tree2->treeID)));

    // The old tree is gone and has 0 nodes.
    setDynArray(state->skeletonState->nodeCounter,
                tree2->treeID,
                (void *)0);

    //Delete the "empty" tree 2
    if(tree2 == state->skeletonState->firstTree) {
        state->skeletonState->firstTree = tree2->next;
        tree2->next->previous = NULL;
    }
    else {
        tree2->previous->next = tree2->next;
        if(tree2->next)
            tree2->next->previous = tree2->previous;
    }
    free(tree2);

    state->skeletonState->treeElements--;
    state->skeletonState->skeletonChanged = TRUE;
    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brdd", KIKI_MERGETREE, treeID1, treeID2))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

uint32_t addNodeToSkeletonStruct(struct nodeListElement *node, struct stateInfo *state) {
    struct skeletonDC *currentSkeletonDC;
    struct skeletonDCnode *currentNewSkeletonDCnode;
    Coordinate currentMagPos;

    currentMagPos.x = node->position.x / state->magnification;
    currentMagPos.y = node->position.y / state->magnification;
    currentMagPos.z = node->position.z / state->magnification;

    currentSkeletonDC = (struct skeletonDC *)ht_get(state->skeletonState->skeletonDCs,
                                                    Px2DcCoord(currentMagPos, state));

    if(currentSkeletonDC == HT_FAILURE) {
        currentSkeletonDC = malloc(sizeof(struct skeletonDC));
        memset(currentSkeletonDC, '\0', sizeof(struct skeletonDC));
        ht_put(state->skeletonState->skeletonDCs,
               Px2DcCoord(currentMagPos, state),
               (Byte *)currentSkeletonDC);
    }

    currentNewSkeletonDCnode = malloc(sizeof (struct skeletonDCnode));
    memset(currentNewSkeletonDCnode, '\0', sizeof(struct skeletonDCnode));

    if(!currentSkeletonDC->firstSkeletonDCnode)
        currentSkeletonDC->firstSkeletonDCnode = currentNewSkeletonDCnode;
    else {
        currentNewSkeletonDCnode->next = currentSkeletonDC->firstSkeletonDCnode;
        currentSkeletonDC->firstSkeletonDCnode = currentNewSkeletonDCnode;
    }
    currentNewSkeletonDCnode->node = node;

    return TRUE;
}

uint32_t addSegmentToSkeletonStruct(struct segmentListElement *segment, struct stateInfo *state) {
    uint32_t i;
    struct skeletonDC *currentSkeletonDC;
    struct skeletonDCsegment *currentNewSkeletonDCsegment;
    float segVectorLength = 0.;
    floatCoordinate segVector;
    Coordinate currentSegVectorPoint, lastSegVectorPoint;
    Coordinate curMagTargetPos, curMagSourcePos;

    if(segment) {
        if(segment->flag == 2)
            return FALSE;
    }
    else
        return FALSE;

    curMagTargetPos.x = segment->target->position.x / state->magnification;
    curMagTargetPos.y = segment->target->position.y / state->magnification;
    curMagTargetPos.z = segment->target->position.z / state->magnification;
    curMagSourcePos.x = segment->source->position.x / state->magnification;
    curMagSourcePos.y = segment->source->position.y / state->magnification;
    curMagSourcePos.z = segment->source->position.z / state->magnification;

    segVector.x = (float)(curMagTargetPos.x - curMagSourcePos.x);
    segVector.y = (float)(curMagTargetPos.y - curMagSourcePos.y);
    segVector.z = (float)(curMagTargetPos.z - curMagSourcePos.z);

    segVectorLength = euclidicNorm(&segVector);
    normalizeVector(&segVector);

    SET_COORDINATE(lastSegVectorPoint,
                   curMagSourcePos.x,
                   curMagSourcePos.y,
                   curMagSourcePos.z);

    //Add the segment pointer to the first skeleton DC
    //Is there a skeleton DC for the first skeleton DC to which a pointer has to be added?
    currentSkeletonDC = (struct skeletonDC *)ht_get(state->skeletonState->skeletonDCs,
                                                    Px2DcCoord(curMagSourcePos, state));
    if(currentSkeletonDC == HT_FAILURE) {
        //A skeleton DC is missing
        LOG("Error: a skeleton DC is missing that should be available. You may encounter other errors.");
        return FALSE;
    }
    currentNewSkeletonDCsegment = malloc(sizeof (struct skeletonDCsegment));
    memset(currentNewSkeletonDCsegment, '\0', sizeof(struct skeletonDCsegment));

    if(!currentSkeletonDC->firstSkeletonDCsegment)
        currentSkeletonDC->firstSkeletonDCsegment = currentNewSkeletonDCsegment;
    else {
        currentNewSkeletonDCsegment->next = currentSkeletonDC->firstSkeletonDCsegment;
        currentSkeletonDC->firstSkeletonDCsegment = currentNewSkeletonDCsegment;
    }
    currentNewSkeletonDCsegment->segment = segment;

    //We walk along the entire segment and add pointers to the segment in all skeleton DCs we are passing.
    for(i = 0; i <= ((int)segVectorLength); i++) {
        currentSegVectorPoint.x = curMagSourcePos.x + roundFloat(((float)i) * segVector.x);
        currentSegVectorPoint.y = curMagSourcePos.y + roundFloat(((float)i) * segVector.y);
        currentSegVectorPoint.z = curMagSourcePos.z + roundFloat(((float)i) * segVector.z);

        if(!COMPARE_COORDINATE(Px2DcCoord(lastSegVectorPoint, state), Px2DcCoord(currentSegVectorPoint, state))) {
            //We crossed a skeleton DC boundary, now we have to add a pointer to the segment inside the skeleton DC
            currentSkeletonDC = (struct skeletonDC *)ht_get(state->skeletonState->skeletonDCs, Px2DcCoord(currentSegVectorPoint, state));
            if(currentSkeletonDC == HT_FAILURE) {
                currentSkeletonDC = malloc(sizeof(struct skeletonDC));
                memset(currentSkeletonDC, '\0', sizeof(struct skeletonDC));
                ht_put(state->skeletonState->skeletonDCs, Px2DcCoord(currentSegVectorPoint, state), (Byte *)currentSkeletonDC);
            }
            currentNewSkeletonDCsegment = malloc(sizeof (struct skeletonDCsegment));
            memset(currentNewSkeletonDCsegment, '\0', sizeof(struct skeletonDCsegment));

            if(!currentSkeletonDC->firstSkeletonDCsegment)
                currentSkeletonDC->firstSkeletonDCsegment = currentNewSkeletonDCsegment;
            else {
                currentNewSkeletonDCsegment->next = currentSkeletonDC->firstSkeletonDCsegment;
                currentSkeletonDC->firstSkeletonDCsegment = currentNewSkeletonDCsegment;
            }
            currentNewSkeletonDCsegment->segment = segment;

            lastSegVectorPoint.x = currentSegVectorPoint.x;
            lastSegVectorPoint.y = currentSegVectorPoint.y;
            lastSegVectorPoint.z = currentSegVectorPoint.z;
        }
    }

    return TRUE;
}

uint32_t delNodeFromSkeletonStruct(struct nodeListElement *node, struct stateInfo *state) {
    struct skeletonDC *currentSkeletonDC;
    struct skeletonDCnode *currentSkeletonDCnode, *lastSkeletonDCnode = NULL;
    Coordinate curMagPos;

    curMagPos.x = node->position.x / state->magnification;
    curMagPos.y = node->position.y / state->magnification;
    curMagPos.z = node->position.z / state->magnification;

    currentSkeletonDC = (struct skeletonDC *)ht_get(state->skeletonState->skeletonDCs,
                                                    Px2DcCoord(curMagPos, state));
    if(currentSkeletonDC == HT_FAILURE) {
        //A skeleton DC is missing
        LOG("Error: a skeleton DC is missing that should be available. You may encounter other errors.");
        return FALSE;
    }

    currentSkeletonDCnode = currentSkeletonDC->firstSkeletonDCnode;

    while(currentSkeletonDCnode) {
        //We found the node we want to delete..
        if(currentSkeletonDCnode->node == node) {
            if(lastSkeletonDCnode != NULL)
                lastSkeletonDCnode->next = currentSkeletonDCnode->next;
            else
                currentSkeletonDC->firstSkeletonDCnode = currentSkeletonDCnode->next;

            free(currentSkeletonDCnode);

            break;
        }

        lastSkeletonDCnode = currentSkeletonDCnode;
        currentSkeletonDCnode = currentSkeletonDCnode->next;
    }

    return TRUE;
}

uint32_t delSegmentFromSkeletonStruct(struct segmentListElement *segment, struct stateInfo *state) {
    uint32_t i;
    struct skeletonDC *currentSkeletonDC;
    struct skeletonDCsegment *lastSkeletonDCsegment, *currentSkeletonDCsegment;
    float segVectorLength = 0.;
    floatCoordinate segVector;
    Coordinate currentSegVectorPoint, lastSegVectorPoint;
    Coordinate curMagTargetPos, curMagSourcePos;

    if(segment) {
        if(segment->flag == 2)
            return FALSE;
    }
    else
        return FALSE;

    curMagTargetPos.x = segment->target->position.x / state->magnification;
    curMagTargetPos.y = segment->target->position.y / state->magnification;
    curMagTargetPos.z = segment->target->position.z / state->magnification;
    curMagSourcePos.x = segment->source->position.x / state->magnification;
    curMagSourcePos.y = segment->source->position.y / state->magnification;
    curMagSourcePos.z = segment->source->position.z / state->magnification;

    segVector.x = (float)(curMagTargetPos.x - curMagSourcePos.x);
    segVector.y = (float)(curMagTargetPos.y - curMagSourcePos.y);
    segVector.z = (float)(curMagTargetPos.z - curMagSourcePos.z);

    segVectorLength = euclidicNorm(&segVector);
    normalizeVector(&segVector);

    SET_COORDINATE(lastSegVectorPoint,
                   curMagSourcePos.x,
                   curMagSourcePos.y,
                   curMagSourcePos.z);

    //Is there a skeleton DC for the first skeleton DC from which the segment has to be removed?
    currentSkeletonDC = (struct skeletonDC *)ht_get(state->skeletonState->skeletonDCs,
                                                    Px2DcCoord(curMagSourcePos, state));
    if(currentSkeletonDC == HT_FAILURE) {
        //A skeleton DC is missing
        LOG("Error: a skeleton DC is missing that should be available. You may encounter other errors.");
        return FALSE;
    }

    lastSkeletonDCsegment = NULL;
    currentSkeletonDCsegment = currentSkeletonDC->firstSkeletonDCsegment;
    while(currentSkeletonDCsegment) {
        //We found the segment we want to delete..
        if(currentSkeletonDCsegment->segment == segment) {
            //Our segment is the first segment
            if(lastSkeletonDCsegment == NULL)
                currentSkeletonDC->firstSkeletonDCsegment = currentSkeletonDCsegment->next;
            else
                lastSkeletonDCsegment->next = currentSkeletonDCsegment->next;

            free(currentSkeletonDCsegment);
            break;
        }

        lastSkeletonDCsegment = currentSkeletonDCsegment;
        currentSkeletonDCsegment = currentSkeletonDCsegment->next;
    }

    //We walk along the entire segment and delete pointers to the segment in all skeleton DCs we are passing.
    for(i = 0; i <= ((int)segVectorLength); i++) {
        currentSegVectorPoint.x = curMagSourcePos.x + roundFloat(((float)i) * segVector.x);
        currentSegVectorPoint.y = curMagSourcePos.y + roundFloat(((float)i) * segVector.y);
        currentSegVectorPoint.z = curMagSourcePos.z + roundFloat(((float)i) * segVector.z);

        if(!COMPARE_COORDINATE(Px2DcCoord(lastSegVectorPoint, state), Px2DcCoord(currentSegVectorPoint, state))) {
            //We crossed a skeleton DC boundary, now we have to delete the pointer to the segment inside the skeleton DC
            currentSkeletonDC = (struct skeletonDC *)ht_get(state->skeletonState->skeletonDCs,
                                                            Px2DcCoord(currentSegVectorPoint, state));
            if(currentSkeletonDC == HT_FAILURE) {
                //A skeleton DC is missing
                LOG("Error: a skeleton DC is missing that should be available. You may encounter other errors.");
                return FALSE;
            }

            lastSkeletonDCsegment = NULL;
            currentSkeletonDCsegment = currentSkeletonDC->firstSkeletonDCsegment;
            while(currentSkeletonDCsegment) {
                //We found the segment we want to delete..
                if(currentSkeletonDCsegment->segment == segment) {
                    //Our segment is the first segment
                    if(lastSkeletonDCsegment == NULL)
                        currentSkeletonDC->firstSkeletonDCsegment = currentSkeletonDCsegment->next;
                    else
                        lastSkeletonDCsegment->next = currentSkeletonDCsegment->next;

                    free(currentSkeletonDCsegment);
                    break;
                }

                lastSkeletonDCsegment = currentSkeletonDCsegment;
                currentSkeletonDCsegment = currentSkeletonDCsegment->next;
            }

            lastSegVectorPoint.x = currentSegVectorPoint.x;
            lastSegVectorPoint.y = currentSegVectorPoint.y;
            lastSegVectorPoint.z = currentSegVectorPoint.z;
        }
    }

    return TRUE;
}

void *popStack(struct stack *stack) {
    /*
     *  The stack should hold values != NULL only, NULL indicates an error.
     */

    void *element = NULL;

    if(stack->stackpointer >= 0)
        element = stack->elements[stack->stackpointer];
    else
        return NULL;

    stack->stackpointer--;
    stack->elementsOnStack--;

    return element;
}

int32_t pushStack(struct stack *stack, void *element) {
    if(element == NULL) {
        LOG("Stack can't hold NULL.");
        return FALSE;
    }

    if(stack->stackpointer + 1 == stack->size) {
        stack->elements = realloc(stack->elements, stack->size * 2 * sizeof(void *));
        if(stack->elements == NULL) {
            LOG("Out of memory.");
            _Exit(FALSE);
        }
        stack->size = stack->size * 2;
    }

    stack->stackpointer++;
    stack->elementsOnStack++;

    stack->elements[stack->stackpointer] = element;

    return TRUE;
}

struct stack *newStack(int32_t size) {
    struct stack *newStack = NULL;

    if(size <= 0) {
        LOG("That doesn't really make any sense, right? Cannot create stack with size <= 0.");
        return NULL;
    }

    newStack = malloc(sizeof(struct stack));
    if(newStack == NULL) {
        LOG("Out of memory.");
        _Exit(FALSE);
    }
    memset(newStack, '\0', sizeof(struct stack));

    newStack->elements = malloc(sizeof(void *) * size);
    if(newStack->elements == NULL) {
        LOG("Out of memory.");
        _Exit(FALSE);
    }
    memset(newStack->elements, '\0', sizeof(void *) * size);

    newStack->size = size;
    newStack->stackpointer = -1;
    newStack->elementsOnStack = 0;

    return newStack;
}

int32_t delStack(struct stack *stack) {
    free(stack->elements);
    free(stack);

    return TRUE;
}

struct dynArray *newDynArray(int32_t size) {
    struct dynArray *newArray = NULL;

    newArray = malloc(sizeof(struct dynArray));
    if(newArray == NULL) {
        LOG("Out of memory.");
        _Exit(FALSE);
    }
    memset(newArray, '\0', sizeof(struct dynArray));

    newArray->elements = malloc(sizeof(void *) * size);
    if(newArray->elements == NULL) {
        LOG("Out of memory.");
        _Exit(FALSE);
    }
    memset(newArray->elements, '\0', sizeof(void *) * size);

    newArray->end = size - 1;
    newArray->firstSize = size;

    return newArray;
}

int32_t setDynArray(struct dynArray *array, int32_t pos, void *value) {
    while(pos > array->end) {
        array->elements = realloc(array->elements, (array->end + 1 +
                                  array->firstSize) * sizeof(void *));
        if(array->elements == NULL) {
            LOG("Out of memory.");
            _Exit(FALSE);
        }
        memset(&(array->elements[array->end + 1]), '\0', array->firstSize);
        array->end = array->end + array->firstSize;
    }

    array->elements[pos] = value;

    return TRUE;
}

void *getDynArray(struct dynArray *array, int32_t pos) {
    if(pos > array->end)
        return FALSE;

    return array->elements[pos];
}

int32_t delDynArray(struct dynArray *array) {
    free(array->elements);
    free(array);

    return TRUE;
}

int32_t splitConnectedComponent(int32_t targetRevision,
                                int32_t nodeID,
                                struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    struct stack *remainingNodes = NULL;
    struct stack *componentNodes = NULL;
    struct nodeListElement *n = NULL, *last = NULL, *node = NULL;
    struct treeListElement *newTree = NULL, *currentTree = NULL;
    struct segmentListElement *currentEdge = NULL;
    struct dynArray *treesSeen = NULL;
    int32_t treesCount = 0;
    int32_t i = 0, id;
    PTRSIZEINT nodeCountAllTrees = 0, nodeCount = 0;
    uint32_t visitedBase, index;
    Byte *visitedRight = NULL, *visitedLeft = NULL, **visited = NULL;
    uint32_t visitedRightLen = 16384, visitedLeftLen = 16384, *visitedLen = NULL;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    /*
     *  This function takes a node and splits the connected component of the
     *  corresponding tree containing that node into a new tree, unless the
     *  connected component is equivalent to the entire tree.
     *
     *  It used depth-first search. Breadth-first-search would be the same with
     *  a queue instead of a stack for storing pending nodes. There is no
     *  difference between the two algorithms for this task.
     *
     *  TODO trees might become empty when the connected component spans more
     *       than one tree.
     */

    /*
     *  This stack can be rather small without ever having to be resized as
     *  our graphs are usually very sparse.
     */

    node = findNodeByNodeID(nodeID, state);
    if(!node) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    remainingNodes = newStack(512);
    componentNodes = newStack(4096);

    /*
     *  This is used to keep track of which trees we've seen. The connected
     *  component for a specific node can be part of more than one tree. That
     *  makes it slightly tedious to check whether the connected component is a
     *  strict subgraph.
     */

    treesSeen = newDynArray(16);

    /*
     *  These are used to store which nodes have been visited.
     *  Nodes with IDs smaller than the entry node will be stored in
     *  visitedLeft, nodes with equal or larger ID will be stored in
     *  visitedRight. This is done to save some memory. ;)
     */

    visitedRight = malloc(16384 * sizeof(Byte));
    visitedLeft = malloc(16384 * sizeof(Byte));

    if(visitedRight == NULL || visitedLeft == NULL) {
        LOG("Out of memory.");
        _Exit(FALSE);
    }

    memset(visitedLeft, NODE_PRISTINE, 16384);
    memset(visitedRight, NODE_PRISTINE, 16384);

    visitedBase = node->nodeID;
    pushStack(remainingNodes, (void *)node);

    // popStack() returns NULL when the stack is empty.
    while((n = (struct nodeListElement *)popStack(remainingNodes))) {
        if(n->nodeID >= visitedBase) {
            index = n->nodeID - visitedBase;
            visited = &visitedRight;
            visitedLen = &visitedRightLen;
        }
        else {
            index = visitedBase - n->nodeID - 1;
            visited = &visitedLeft;
            visitedLen = &visitedLeftLen;
        }

        // If the index is out bounds of the visited array, we resize the array,
        // 4096 bytes at a time, until it is large enough.
        while(index > *visitedLen) {
            *visited = realloc(*visited, (*visitedLen + 16384) * sizeof(Byte));
            if(*visited == NULL) {
                LOG("Out of memory.");
                _Exit(FALSE);
            }

            memset(*visited + *visitedLen, NODE_PRISTINE, 16384 * sizeof(Byte));
            *visitedLen = *visitedLen + 16384;
        }

        if((*visited)[index] == NODE_VISITED)
            continue;

        (*visited)[index] = NODE_VISITED;

        pushStack(componentNodes, (void *)n);

        // Check if the node is in a tree we haven't yet seen.
        // If it is, add that tree as seen.
        for(i = 0; i < treesCount; i++) {
            if(getDynArray(treesSeen, i) == (void *)n->correspondingTree)
                break;
        }
        if(i == treesCount) {
            setDynArray(treesSeen, treesCount, (void *)n->correspondingTree);
            treesCount++;
        }

        nodeCount = nodeCount + 1;

        // And now we push all adjacent nodes on the stack.
        currentEdge = n->firstSegment;
        while(currentEdge) {
            if(currentEdge->flag == SEGMENT_FORWARD)
                pushStack(remainingNodes, (void *)currentEdge->target);
            else if(currentEdge->flag == SEGMENT_BACKWARD)
                pushStack(remainingNodes, (void *)currentEdge->source);

            currentEdge = currentEdge->next;
        }
    }

    /*
     *  If the total number of nodes visited is smaller than the sum of the
     *  number of nodes in all trees we've seen, the connected component is a
     *  strict subgraph of the graph containing all trees we've seen and we
     *  should split it.
     */

    for(i = 0; i < treesCount; i++) {
        currentTree = (struct treeListElement *)getDynArray(treesSeen, i);
        id = currentTree->treeID;
        nodeCountAllTrees += (PTRSIZEINT)getDynArray(state->skeletonState->nodeCounter,
                                                     id);
    }

    if(nodeCount < nodeCountAllTrees) {
        color4F treeCol;
        treeCol.r = -1.;
        newTree = addTreeListElement(FALSE, CHANGE_MANUAL, 0, treeCol);
        // Splitting the connected component.

        while((n = (struct nodeListElement *)popStack(componentNodes))) {
            // Removing node list element from its old position
            setDynArray(state->skeletonState->nodeCounter,
                        n->correspondingTree->treeID,
                        (void *)((PTRSIZEINT)getDynArray(state->skeletonState->nodeCounter,
                                                         n->correspondingTree->treeID) - 1));

            if(n->previous != NULL)
                n->previous->next = n->next;
            else
                n->correspondingTree->firstNode = n->next;

            if(n->next != NULL)
                n->next->previous = n->previous;

            // Inserting node list element into new list.
            setDynArray(state->skeletonState->nodeCounter, newTree->treeID,
                        (void *)((PTRSIZEINT)getDynArray(state->skeletonState->nodeCounter,
                                                         newTree->treeID) + 1));

            n->next = NULL;

            if(last != NULL) {
                n->previous = last;
                last->next = n;
            }
            else {
                n->previous = NULL;
                newTree->firstNode = n;
            }

            last = n;

            n->correspondingTree = newTree;
        }


        state->skeletonState->skeletonChanged = TRUE;

    }
    else
        LOG("The connected component is equal to the entire tree, not splitting.");

    delStack(remainingNodes);
    delStack(componentNodes);

    free(visitedLeft);
    free(visitedRight);

    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brd", KIKI_SPLIT_CC, nodeID)) {
            LOG("broken in splitcc.");
            skeletonSyncBroken(state);
        }
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return nodeCount;
}

uint32_t addComment(int32_t targetRevision, char *content, struct nodeListElement *node, int32_t nodeID, struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    struct commentListElement *newComment;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    newComment = malloc(sizeof(struct commentListElement));
    memset(newComment, '\0', sizeof(struct commentListElement));

    newComment->content = malloc(strlen(content) * sizeof(char) + 1);
    memset(newComment->content, '\0', strlen(content) * sizeof(char) + 1);

    if(nodeID)
        node = findNodeByNodeID(nodeID, state);

    if(node) {
        newComment->node = node;
        node->comment = newComment;
    }

    if(content)
        strncpy(newComment->content, content, strlen(content));

    if(!state->skeletonState->currentComment) {
        state->skeletonState->currentComment = newComment;
        //We build a circular linked list
        newComment->next = newComment;
        newComment->previous = newComment;
    }
    else {
        //We insert into a circular linked list
        state->skeletonState->currentComment->previous->next = newComment;
        newComment->next = state->skeletonState->currentComment;
        newComment->previous = state->skeletonState->currentComment->previous;
        state->skeletonState->currentComment->previous = newComment;

        state->skeletonState->currentComment = newComment;
    }

    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "blrds", KIKI_ADDCOMMENT, node->nodeID, content))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

uint32_t editComment(int32_t targetRevision,
                     struct commentListElement *currentComment,
                     int32_t nodeID,
                     char *newContent,
                     struct nodeListElement *newNode,
                     int32_t newNodeID,
                     struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */
    // this function also seems to be kind of useless as you could do just the same
    // thing with addComment() with minimal changes ....?

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    if(nodeID)
        currentComment = findNodeByNodeID(nodeID, state)->comment;

    if(!currentComment) {
        LOG("Please provide a valid comment element to edit!");
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    nodeID = currentComment->node->nodeID;

    if(newContent) {
        if(currentComment->content)
            free(currentComment->content);
        currentComment->content = malloc(strlen(newContent) * sizeof(char) + 1);
        memset(currentComment->content, '\0', strlen(newContent) * sizeof(char) + 1);
        strncpy(currentComment->content, newContent, strlen(newContent));
    }

    if(newNodeID)
        newNode = findNodeByNodeID(newNodeID, state);

    if(newNode) {
        if(currentComment->node)
            currentComment->node->comment = NULL;

        currentComment->node = newNode;
        newNode->comment = currentComment;
    }

    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "blrdds",
                    KIKI_EDITCOMMENT,
                    nodeID,
                    currentComment->node->nodeID,
                    newContent))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

uint32_t delComment(int32_t targetRevision, struct commentListElement *currentComment, int32_t commentNodeID, struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    int32_t nodeID = 0;
    struct nodeListElement *commentNode = NULL;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    if(commentNodeID) {
        commentNode = findNodeByNodeID(commentNodeID, state);
        if(commentNode)
            currentComment = commentNode->comment;
    }

    if(!currentComment) {
        LOG("Please provide a valid comment element to delete!");
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    if(currentComment->content)
        free(currentComment->content);
    if(currentComment->node) {
        nodeID = currentComment->node->nodeID;
        currentComment->node->comment = NULL;
    }

    if(state->skeletonState->currentComment == currentComment) {
        memset(state->skeletonState->commentBuffer, '\0', 10240);
    }

    if(currentComment->next == currentComment) {
        state->skeletonState->currentComment = NULL;
    }
    else {
        currentComment->next->previous = currentComment->previous;
        currentComment->previous->next = currentComment->next;

        if(state->skeletonState->currentComment == currentComment) {
            state->skeletonState->currentComment = currentComment->next;
        }
    }

    free(currentComment);

    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brd", KIKI_DELCOMMENT, nodeID))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

//Changing the "active comment" will currently change the active node - this behaviour may change...
struct commentListElement *nextComment(char *searchString, struct stateInfo *state) {
    struct commentListElement *firstComment, *currentComment;

    if(!strlen(searchString)) {
        //->previous here because it would be unintuitive for the user otherwise.
        //(we insert new comments always as first elements)
        if(state->skeletonState->currentComment) {
            state->skeletonState->currentComment = state->skeletonState->currentComment->previous;
            setActiveNode(CHANGE_MANUAL,
                          state->skeletonState->currentComment->node,
                          0);
            jumpToActiveNode(state);
        }
    }
    else {
        if(state->skeletonState->currentComment) {
            firstComment = state->skeletonState->currentComment->previous;
            currentComment = firstComment;
            do {
                if(strstr(currentComment->content, searchString) != NULL) {
                    state->skeletonState->currentComment = currentComment;
                    setActiveNode(CHANGE_MANUAL,
                                  state->skeletonState->currentComment->node,
                                  0);
                    jumpToActiveNode(state);
                    break;
                }
                currentComment = currentComment->previous;

            } while (firstComment != currentComment);
        }
    }

    memset(state->skeletonState->commentBuffer, '\0', 10240);

    if(state->skeletonState->currentComment) {
        strncpy(state->skeletonState->commentBuffer,
                state->skeletonState->currentComment->content,
                strlen(state->skeletonState->currentComment->content));

        if(state->skeletonState->lockPositions) {
            if(strstr(state->skeletonState->commentBuffer, state->skeletonState->onCommentLock))
                lockPosition(state->skeletonState->currentComment->node->position, state);
            else
                unlockPosition(state);
        }

    }

    drawGUI(state);

    return state->skeletonState->currentComment;
}

int32_t nextCommentlessNode() {

    return TRUE;
}

int32_t previousCommentlessNode() {
    struct nodeListElement *currNode;
    currNode = state->skeletonState->activeNode->previous;
    if(currNode && (!currNode->comment)) {
        LOG("yes");
        setActiveNode(CHANGE_MANUAL,
                      currNode,
                      0);
        jumpToActiveNode(state);
    }
    return TRUE;
}

int32_t lockPosition(Coordinate lockCoordinate, struct stateInfo *state) {
    LOG("locking to (%d, %d, %d).", lockCoordinate.x, lockCoordinate.y, lockCoordinate.z);
    state->skeletonState->positionLocked = TRUE;
    SET_COORDINATE(state->skeletonState->lockedPosition,
                   lockCoordinate.x,
                   lockCoordinate.y,
                   lockCoordinate.z);

    return TRUE;
}

int32_t unlockPosition(struct stateInfo *state) {
    if(state->skeletonState->positionLocked)
        LOG("Spatial locking disabled.");

    state->skeletonState->positionLocked = FALSE;

    return TRUE;
}

struct commentListElement *previousComment(char *searchString, struct stateInfo *state) {
    struct commentListElement *firstComment, *currentComment;
    // ->next here because it would be unintuitive for the user otherwise.
    // (we insert new comments always as first elements)

    if(!strlen(searchString)) {
        if(state->skeletonState->currentComment) {
            state->skeletonState->currentComment = state->skeletonState->currentComment->next;
            setActiveNode(CHANGE_MANUAL,
                          state->skeletonState->currentComment->node,
                          0);
            jumpToActiveNode(state);
        }
    }
    else {
        if(state->skeletonState->currentComment) {
            firstComment = state->skeletonState->currentComment->next;
            currentComment = firstComment;
            do {
                if(strstr(currentComment->content, searchString) != NULL) {
                    state->skeletonState->currentComment = currentComment;
                    setActiveNode(CHANGE_MANUAL,
                                  state->skeletonState->currentComment->node,
                                  0);
                    jumpToActiveNode(state);
                    break;
                }
                currentComment = currentComment->next;

            } while (firstComment != currentComment);

        }
    }
    memset(state->skeletonState->commentBuffer, '\0', 10240);

    if(state->skeletonState->currentComment) {
        strncpy(state->skeletonState->commentBuffer,
            state->skeletonState->currentComment->content,
            strlen(state->skeletonState->currentComment->content));

        if(state->skeletonState->lockPositions) {
            if(strstr(state->skeletonState->commentBuffer, state->skeletonState->onCommentLock))
                lockPosition(state->skeletonState->currentComment->node->position, state);
            else
                unlockPosition(state);
        }
    }


    drawGUI(state);

    return state->skeletonState->currentComment;
}

uint32_t searchInComment(char *searchString, struct commentListElement *comment, struct stateInfo *state) {

    return TRUE;
}


int32_t pushBranchNode(int32_t targetRevision,
                       int32_t setBranchNodeFlag,
		       int32_t checkDoubleBranchpoint,
                       struct nodeListElement *branchNode,
                       int32_t branchNodeID,
                       struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    if(branchNodeID)
        branchNode = findNodeByNodeID(branchNodeID, state);

    if(branchNode) {
        if(branchNode->isBranchNode == 0 || !checkDoubleBranchpoint) {
            pushStack(state->skeletonState->branchStack, (void *)(PTRSIZEINT)branchNode->nodeID);
            if(setBranchNodeFlag)
                branchNode->isBranchNode = TRUE;
            state->skeletonState->skeletonChanged = TRUE;
            LOG("Branch point (node ID %d) added.", branchNode->nodeID);
        }
        else {
            LOG("Active node is already a branch point");
            unlockSkeleton(TRUE, state);
            return TRUE;
        }
    }
    else {
        LOG("Make a node active before adding branch points.");
        unlockSkeleton(TRUE, state);
        return TRUE;
    }

    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "brd", KIKI_PUSHBRANCH, branchNode->nodeID))
            skeletonSyncBroken(state);
    }
    else
        refreshViewports(state);

    unlockSkeleton(TRUE, state);

    return TRUE;
}

void UI_popBranchNode() {
    /* Inconsistency:
       Confirmation will not be asked when no branch points remain, except if the remaining
       branch point nodes don't exist anymore (nodes have been deleted). */

    /* This is workaround around agar bug #171 */
    if(state->skeletonState->askingPopBranchConfirmation == FALSE) {
        state->skeletonState->askingPopBranchConfirmation = TRUE;

        if(state->skeletonState->branchpointUnresolved && state->skeletonState->branchStack->stackpointer != -1) {
            yesNoPrompt(NULL,
                        "No node was added after jumping to the last branch point. Do you really want to jump?",
                        WRAP_popBranchNode,
                        popBranchNodeCanceled);
        }
        else
            WRAP_popBranchNode();
    }

    return;
}

static void WRAP_popBranchNode() {
    popBranchNode(CHANGE_MANUAL, state);
    state->skeletonState->askingPopBranchConfirmation = FALSE;
}

static void popBranchNodeCanceled() {
    state->skeletonState->askingPopBranchConfirmation = FALSE;
}

int32_t popBranchNode(int32_t targetRevision, struct stateInfo *state) {
    /* This is a SYNCHRONIZABLE skeleton function. Be a bit careful. */
    /* SYNCHRO BUG:
       both instances will have to confirm branch point deletion if
       confirmation is asked.
    */

    struct nodeListElement *branchNode = NULL;
    PTRSIZEINT branchNodeID;

    if(lockSkeleton(targetRevision, state) == FALSE) {
        unlockSkeleton(FALSE, state);
        return FALSE;
    }

    /* Nodes on the branch stack may not actually exist anymore */
    while(branchNode == NULL) {
        branchNodeID = (PTRSIZEINT)popStack(state->skeletonState->branchStack);
        if(branchNodeID == 0) {
            AG_TextMsg(AG_MSG_INFO, "No branch points remain.");
            LOG("No branch points remain.");

            goto exit_popbranchnode;
        }

        branchNode = findNodeByNodeID(branchNodeID, state);
    }

    if(branchNode && branchNode->isBranchNode) {
#ifdef ARCH_64
        LOG("Branch point (node ID %"PRId64") deleted.", branchNodeID);
#else
        LOG("Branch point (node ID %d) deleted.", branchNodeID);
#endif

        tempConfig->viewerState->currentPosition.x
            = branchNode->position.x / state->magnification;
        tempConfig->viewerState->currentPosition.y
            = branchNode->position.y / state->magnification;
        tempConfig->viewerState->currentPosition.z
            = branchNode->position.z / state->magnification;


        setActiveNode(CHANGE_NOSYNC, branchNode, 0);

        branchNode->isBranchNode--;
        state->skeletonState->skeletonChanged = TRUE;

        updatePosition(state, TELL_COORDINATE_CHANGE);

        state->skeletonState->branchpointUnresolved = TRUE;
    }

exit_popbranchnode:

    state->skeletonState->unsavedChanges = TRUE;
    state->skeletonState->skeletonRevision++;

    if(targetRevision == CHANGE_MANUAL) {
        if(!syncMessage(state, "br", KIKI_POPBRANCH))
            skeletonSyncBroken(state);
    }
    else {
        refreshViewports(state);
    }

    unlockSkeleton(TRUE, state);
    return TRUE;
}

int32_t jumpToActiveNode() {
    if(state->skeletonState->activeNode) {
        tempConfig->viewerState->currentPosition.x =
            state->skeletonState->activeNode->position.x /
            state->magnification;
        tempConfig->viewerState->currentPosition.y =
            state->skeletonState->activeNode->position.y /
            state->magnification;
        tempConfig->viewerState->currentPosition.z =
            state->skeletonState->activeNode->position.z /
            state->magnification;

        updatePosition(state, TELL_COORDINATE_CHANGE);
    }

    return TRUE;
}
