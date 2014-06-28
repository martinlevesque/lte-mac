// BasicLTE.cc
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

#include "EPON_CtrlInfo.h"
#include "BasicLTE.h"
#include "Lambda.h"
#include "PONUtil.h"
#include <string>
#include "MyUtil.h"
#include "SimpleQueue.h"
#include "LTEMessages_m.h"

using namespace std;

Define_Module(BasicLTE);

void BasicLTE::initialize(int stage)
{
    if (stage != 4)
        return;

    cModule* tmp = MyUtil::findModuleUp(this, "wlan");

    if (tmp)
    {
        BasicIeee80211Mac* t2 = dynamic_cast<BasicIeee80211Mac*>(MyUtil::findModuleUp(tmp, "mac"));

        if (t2)
        {
            myAddr = t2->getMACAddress();
        }
    }

    confsAdjLTENodes();

    LTENodeType = par("LTENodeType").stringValue();
    upstreamCapacity = par("upstreamCapacity").doubleValue();
    downstreamCapacity = par("downstreamCapacity").doubleValue();
    distance = par("distance").doubleValue();
    TTIDuration = par("TTIDuration").doubleValue();
    TTISlot = new cMessage("TTISlot");

    nbContentionPreambles = par("nbContentionPreambles");
    slotAlohaDuration = par("slotAlohaDuration").doubleValue();
    uplinkNbRessourceBlocks = par("uplinkNbRessourceBlocks");
    maxCONTXAttemps = par("maxCONTXAttemps");
    backoffCONUniform = par("backoffCONUniform");

    bitsPerRessourceBlock = (int)((upstreamCapacity * TTIDuration) / (double)uplinkNbRessourceBlocks);

    EV << "bitsPerRessourceBlock = " << bitsPerRessourceBlock << endl;

    msgBackoff = new cMessage("msgBackoff");

    msgAfterPreambleRequests = new cMessage("msgAfterPreambleRequests");

    nbCONAttemps = 0;
    UEConnected = false;

    UEBitsAllocConnection = 0;
    UECurBitsRcv = 0;

    EV << "nbContentionPreambles = " << nbContentionPreambles << endl;
    EV << "slotAlohaDuration = " << slotAlohaDuration << endl;
    EV << "uplinkNbRessourceBlocks = " << uplinkNbRessourceBlocks << endl;

    msgDownstreamSched = new cMessage("msgDownstreamSched");
    downstreamQueue = new SimpleQueue();
    downstreamQueue->setLimit(200000000);
    downstreamQueue->initialize(this);

    upstreamQueue = new SimpleQueue();
    upstreamQueue->setLimit(200000000);
    upstreamQueue->initialize(this);

    // Sched the TTI:
    scheduleAt(TTIDuration, TTISlot);
}

void BasicLTE::confsAdjLTENodes()
{
    int ports = gateSize("ports");

    for (int i = 0; i < ports; ++i)
    {
        cModule* node = MyUtil::getNeighbourOnGate(this, "ports$o", i);

        if (node)
        {
            node = MyUtil::getNeighbourOnGate(node, "ltePorts$o", i);

            if (node)
            {
                cModule* wlan = MyUtil::findModuleUp(node, "wlan");

                if (wlan)
                {
                    BasicIeee80211Mac* mac = dynamic_cast<BasicIeee80211Mac*>(MyUtil::findModuleUp(wlan, "mac"));

                    if (mac)
                    {
                        adjLTENodes[mac->getMACAddress().str()] = i;
                    }
                }
            }
        }
    }
}

BasicLTE* BasicLTE::findUEWith(const std::string& ueMACAddr)
{
    for (std::map<std::string, int>::iterator it = adjLTENodes.begin(); it != adjLTENodes.end(); ++it)
    {
        if (it->first == ueMACAddr)
        {
            cModule* adjNode = MyUtil::getNeighbourOnGate(this, "ports$o", it->second);

            if (adjNode)
            {
                BasicLTE* lteMod = dynamic_cast<BasicLTE*>(MyUtil::findModuleUp(adjNode, "lte"));

                return lteMod;
            }
        }
    }

    return NULL;
}

bool BasicLTE::routeInZone(EtherFrame* frame)
{
    // adjLTENodes

    for (std::map<std::string, int>::iterator it = adjLTENodes.begin(); it != adjLTENodes.end(); ++it)
    {
        if (it->first == frame->getDest().str())
        {
            send(frame, gate("ports$o", it->second));
            return true;
        }
    }

    return false;
}

double BasicLTE::durationTXDownstream(EtherFrame* frame)
{
    double TX = (double)frame->getBitLength() / downstreamCapacity;
    double prop = distance / 300000.0; // speed of light.

    return TX + prop;
}

bool BasicLTE::checkArrivedAtDest(EtherFrame* frame)
{
    if ( ! frame)
        return false;

    if (myAddr == frame->getDest())
    {
        // Forward to cli !
        send(frame, gate("cli$o"));
        return true;
    }

    return false;
}

int BasicLTE::genRandomPreamble()
{
    return (int)uniform(0, nbContentionPreambles);
}

int BasicLTE::genRandomBackoff()
{
    return (int)uniform(0, backoffCONUniform);
}

void BasicLTE::handleSelfMessage(cMessage* msg)
{
    if (dynamic_cast<EtherFrame*>(msg))
    {
        if (LTENodeType == "UE")
        {
            send(msg, "ports$o", 0);
            return;
        }
    }
    else
    if (msg == msgDownstreamSched)
    {
        EtherFrame* frame = dynamic_cast<EtherFrame*>(downstreamQueue->front(100000));

        if ( ! frame)
        {
            error("BasicLTE::handleMessage err 1");
        }

        downstreamQueue->remove(frame);
        routeInZone(frame);

        if ( ! downstreamQueue->isEmpty())
        {
            EtherFrame* next = dynamic_cast<EtherFrame*>(downstreamQueue->front(100000));
            scheduleAt(simTime().dbl() + durationTXDownstream(next), msgDownstreamSched);
        }
    }
    else
    if (msg == TTISlot)
    {
        scheduleAt(simTime().dbl() + TTIDuration, TTISlot);

        // We need to process the connection request/response mechanism
        if (upstreamQueue->length() > 0 && ! UEConnected && LTENodeType == "UE" && ! msgBackoff->isScheduled())
        {
            // There are frames waiting and we are not connected: we need to connect

            LTEPreambleConnectionRequest* req = new LTEPreambleConnectionRequest("LTEPreambleConnectionRequest");
            req->setMacAddrSource(myAddr.str().c_str());
            req->setPendingBitsToTX(upstreamQueue->lengthInBytes()*8);
            int preamble = genRandomPreamble();
            req->setSelectedPreamble(preamble);

            nbCONAttemps = 1;
            UEBitsAllocConnection = req->getPendingBitsToTX();

            send(req, gate("ports$o", 0));
        }
        else
        if (LTENodeType == "eNB")
        {
            // Need to create allocations per UE -> bits
            map<string, int> allocations;

            // Init:
            for (map<std::string, ScheduleTransmission>::iterator it = schedTransmissions.begin(); it != schedTransmissions.end(); ++it)
            {
                if (it->second.pendingBits > 0)
                    allocations[it->first] = 0;
            }

            if (allocations.size() > 0)
            {
                map<std::string, ScheduleTransmission>::iterator itTransmissions = schedTransmissions.begin();

                // Allocate each block (of bitsPerRessourceBlock bits)
                for (int i = 0; i < uplinkNbRessourceBlocks; ++i)
                {
                    int nbTimesLoopingToEnd = 0;

                    while (itTransmissions->second.pendingBits <= 0)
                    {
                        ++itTransmissions;

                        if (itTransmissions == schedTransmissions.end())
                        {
                            itTransmissions = schedTransmissions.begin();
                            ++nbTimesLoopingToEnd;

                            if (nbTimesLoopingToEnd >= 2)
                                break;
                        }
                    }

                    if (nbTimesLoopingToEnd >= 2)
                        break;

                    allocations[itTransmissions->first] += bitsPerRessourceBlock;

                    EV << "SCHED ALLOC to " << itTransmissions->first << " total  " << allocations[itTransmissions->first] << endl;

                    itTransmissions->second.pendingBits -= bitsPerRessourceBlock;

                    if (itTransmissions->second.pendingBits < 0)
                        itTransmissions->second.pendingBits = 0;

                    ++itTransmissions;

                    if (itTransmissions == schedTransmissions.end())
                    {
                        itTransmissions = schedTransmissions.begin();
                    }
                }
            }

            for (map<string, int>::iterator it = allocations.begin(); it != allocations.end(); ++it)
            {
                if (it->second <= 0)
                    continue;

                LTEENBSchedTransmission* allocSched = new LTEENBSchedTransmission("LTEENBSchedTransmission");

                allocSched->setMacAddrUE(it->first.c_str());
                allocSched->setAllocatedBits(it->second);

                scheduleAt(simTime().dbl() + TTIDuration, allocSched);
                //
            }
        }
    }
    else
    if (msg == msgAfterPreambleRequests)
    {
        map<string, bool> preamblesValids;

        for (int i = 0; i < (int)eNBRequests.size(); ++i)
        {
            bool preambleCollided = false;

            for (int j = 0; j < (int)eNBRequests.size(); ++j)
            {
                if (i != j)
                {
                    if (eNBRequests[i]->getSelectedPreamble() == eNBRequests[j]->getSelectedPreamble())
                    {
                        preambleCollided = true;
                        break;
                    }
                }
            }

            preamblesValids[string(eNBRequests[i]->getMacAddrSource())] = ! preambleCollided;
        }

        // Clean req messages:
        for (int i = 0; i < (int)eNBRequests.size(); ++i)
        {
            string addr = string(eNBRequests[i]->getMacAddrSource());

            if (preamblesValids[addr])
            {
                schedTransmissions[addr].pendingBits = eNBRequests[i]->getPendingBitsToTX();
            }

            delete eNBRequests[i];
        }

        eNBRequests.clear();

        for (map<string, bool>::iterator it = preamblesValids.begin(); it != preamblesValids.end(); ++it)
        {
            for (std::map<std::string, int>::iterator itNode = adjLTENodes.begin(); itNode != adjLTENodes.end(); ++itNode)
            {
                if (it->first == itNode->first)
                {
                    // send to itNode->second
                    LTEConnectionResponse* response = new LTEConnectionResponse("LTEConnectionResponse");
                    response->setMacAddrUE(it->first.c_str());
                    response->setReceived(it->second);

                    send(response, "ports$o", adjLTENodes[it->first]);
                    break;
                }
            }
        }
    }
    else
    if (dynamic_cast<LTEENBSchedTransmission*>(msg))
    {
        LTEENBSchedTransmission* sched = dynamic_cast<LTEENBSchedTransmission*>(msg);

        send(sched, "ports$o", adjLTENodes[sched->getMacAddrUE()]);
    }
    else
    if (msg == msgBackoff)
    {
        EV << "backoff finished" << endl;
    }
}

void BasicLTE::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
    {
        handleSelfMessage(msg);

        return;
    }

	// Handle frame sent down from the network entity: send out on every other port
	cGate *ingate = msg->getArrivalGate();
	string inGateName = ingate->getName();


	if (inGateName == string("fromWiFi"))
	{
	    EV << "lte FROM WIFI" << endl;
	    EtherFrame* frame = dynamic_cast<EtherFrame*>(msg);

	    // DOWNSTREAM
	    if (LTENodeType == "eNB")
	    {
	        EV << "SUPER WTF" << endl;
            downstreamQueue->enqueue(frame);

            if ( ! msgDownstreamSched->isScheduled())
            {
                scheduleAt(simTime().dbl() + durationTXDownstream(frame), msgDownstreamSched);
            }
            else
            {
                // Just enqueue.
            }
	    }
	    else // Upstream
	    if (LTENodeType == "UE")
	    {
	        EV << "SUPER DUPPER UPSTREAM ! " << endl;
	        upstreamQueue->enqueue(frame);
	    }

	    return;
	}
	else
	if (inGateName == string("cli$i"))
	{
		EV << "BasicLTE: FROM cli" << endl;

		EtherFrame* frame = dynamic_cast<EtherFrame*>(msg);

		if ( ! frame)
		{
		    error("BasicLTE::handleMessage problem frame type cli");
		}

		EV << "Enqueing" << endl;
		upstreamQueue->enqueue(frame);

		// Normally, there is only one port for UEs
		//send(msg, gate("ports$o", 0));
	}
	else
	if (inGateName == "ports$i") // arrived at eNB/UE
	{
	    // READY TO TX

	    EtherFrame* dataFrame = dynamic_cast<EtherFrame*>(msg);

	    if (LTENodeType == "UE")
	    {
	        if (dataFrame)
	        {
                if (checkArrivedAtDest(dataFrame))
                    return;
	        }
	        else
	        if (dynamic_cast<LTEConnectionResponse*>(msg))
	        {
	            EV << "LTEConnectionResponse received !! " << endl;

	            LTEConnectionResponse* response = dynamic_cast<LTEConnectionResponse*>(msg);

	            if (response->getReceived())
	            {
	                // OK, we are connected.
	                UEConnected = true;
	                UECurBitsRcv = 0;
	            }
	            else
	            {
	                UEConnected = false;
	                ++nbCONAttemps;

	                // We collided.

	                if (nbCONAttemps == maxCONTXAttemps) // max attempt.
	                {
	                    nbCONAttemps = 0;
	                }
	                else
	                {
	                    // Backoff
	                    int cw = genRandomBackoff();

	                    if (cw > 0)
	                    {
	                        double backoffDuration = ((double)cw) * slotAlohaDuration;

	                        scheduleAt(simTime().dbl() + backoffDuration, msgBackoff);
	                    }
	                }
	            }

	            delete msg;
	        }
	        else
	        if (dynamic_cast<LTEENBSchedTransmission*>(msg))
	        {
	            EV << "Received LTEENBSchedTransmission " << endl;

	            LTEENBSchedTransmission* sched = dynamic_cast<LTEENBSchedTransmission*>(msg);

	            UECurBitsRcv += sched->getAllocatedBits();
	            EV << "ok LOCAL UECurBitsRcv =  <" << UECurBitsRcv << endl;

	            EtherFrame* frame = dynamic_cast<EtherFrame*>(upstreamQueue->front(1000000));

	            if (UECurBitsRcv >= frame->getBitLength())
	            {
	                // OK we can transmit
	                UECurBitsRcv = UECurBitsRcv - frame->getBitLength();

	                EV << "ready to tx, UECurBitsRcv = " << UECurBitsRcv << endl;

	                scheduleAt(simTime().dbl() + TTIDuration, frame);

	                // and then remove it
	                upstreamQueue->remove(frame);
	            }

	            UEBitsAllocConnection -= sched->getAllocatedBits();

	            if (UEBitsAllocConnection <= 0)
	            {
	                EV << "UEBitsAllocConnection < = 0" << endl;
	                UEBitsAllocConnection = 0;
	                UEConnected = false;
	            }

	            delete sched;
	        }
	    }
	    else
	    if (LTENodeType == "eNB")
	    {
	        if (dataFrame)
	        {
	            EV << "lte arrived at dest 1 " << endl;
	            // Is it the DEST ?
	            if (myAddr == dataFrame->getDest())
	            {
	                EV << "lte arrived at dest 2 " << endl;
	                send(dataFrame, "cli$o");
	                return;
	            }

	            EV << "lte arrived at dest 3 " << endl;

                // Check if it is in the zone
                if (routeInZone(dataFrame))
                {
                    EV << "lte arrived at dest 4 " << endl;
                    return;
                }

                // Otherwise, forward to wlan.uppergateIn
                cModule* wlan = MyUtil::findModuleUp(this, "wlan");

                if ( ! wlan)
                {
                    error("In BasicLTE, no wlan ??");
                }

                cModule* mgmt = MyUtil::findModuleUp(wlan, "mgmt");

                if ( ! mgmt)
                {
                    error("In BasicLTE, no mgmt ??");
                }

                EV << "lte arrived at dest 5 " << endl;

                sendDirect(msg, mgmt->gate("fromLTE"));
	        }
	        else
	        if (dynamic_cast<LTEPreambleConnectionRequest*>(msg))
	        {
	            LTEPreambleConnectionRequest* req = dynamic_cast<LTEPreambleConnectionRequest*>(msg);

	            eNBRequests.push_back(req);

	            if ( ! msgAfterPreambleRequests->isScheduled())
	            {
	                scheduleAt(simTime().dbl() + 0.00001, msgAfterPreambleRequests);
	            }
	        }
	    }
	}
	else
	{
	    delete msg;
	}

}

void BasicLTE::finish ()
{
}
