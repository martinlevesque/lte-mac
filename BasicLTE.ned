//
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

package FiWi.LTE;
//
// TODO auto-generated module
//
simple BasicLTE
{
    parameters:
        @display("i=block/ifcard");
        
        string LTENodeType = default("UE"); // UE, eNB
        double upstreamCapacity = default(10500000); // bps // 1 sector
        double downstreamCapacity = default(17300000); // bps
        double distance = 5.0; // in kms
        
        double TTIDuration = 0.001;
        
        // Contention resolution:
        int nbContentionPreambles = 57;//default(57);
        double slotAlohaDuration = TTIDuration;
        int uplinkNbRessourceBlocks = default(100);
        int maxCONTXAttemps = default(8);
        int backoffCONUniform = default(20); // max of 20 slots of 1 ms each
        
    gates:
        
        inout ports[];
        inout cli;
        
        input fromWiFi @loose;
}
