// BasicLTE.h
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
// @author: Martin Lévesque, <levesque.martin@gmail.com>
//

#ifndef __BASIC_LTE_H__
#define __BASIC_LTE_H__

#include <omnetpp.h>
#include "MPCPTools.h"
#include <map>
#include <vector>
#include "SimpleQueue.h"
#include "LTEMessages_m.h"
#include "BasicLTE.h"

struct ScheduleTransmission
{
    int pendingBits;
};

/**
 */
class BasicLTE : public cSimpleModule
{
  public:

    std::string LTENodeType;

    BasicLTE* findUEWith(const std::string& ueMACAddr);

  protected:
    virtual void initialize(int stage);
    virtual int numInitStages() const {return 5;}
    virtual void handleMessage(cMessage *msg);
    virtual void finish();

    MACAddress myAddr;
    std::map<std::string, int> adjLTENodes;


    double upstreamCapacity;
    double downstreamCapacity;
    double distance;
    double TTIDuration;
    cMessage* TTISlot;

    // Contention:
    int nbContentionPreambles;
    double slotAlohaDuration;
    int uplinkNbRessourceBlocks;
    int bitsPerRessourceBlock;
    int maxCONTXAttemps;
    int backoffCONUniform;
    cMessage* msgBackoff;

    std::vector<LTEPreambleConnectionRequest*> eNBRequests;
    std::map<std::string, ScheduleTransmission> schedTransmissions;
    cMessage* msgAfterPreambleRequests;

    int nbCONAttemps;
    bool UEConnected;

    int UEBitsAllocConnection;
    int UECurBitsRcv;

    // Downstream:
    cMessage* msgDownstreamSched;
    MyAbstractQueue* downstreamQueue;

    // Upstream:
    MyAbstractQueue* upstreamQueue;

    bool routeInZone(EtherFrame* frame);
    double durationTXDownstream(EtherFrame* frame);
    bool checkArrivedAtDest(EtherFrame* frame);
    int genRandomPreamble();
    int genRandomBackoff();
    void handleSelfMessage(cMessage* msg);
    void confsAdjLTENodes();
};



#endif
