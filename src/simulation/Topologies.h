#pragma once


#include "simulation/Simulation.h"

namespace viichain
{

class Topologies
{
  public:
    static Simulation::pointer
    pair(Simulation::Mode mode, Hash const& networkID,
         Simulation::ConfigGen confGen = nullptr,
         Simulation::QuorumSetAdjuster qSetAdjust = nullptr);

        static Simulation::pointer
    cycle4(Hash const& networkID, Simulation::ConfigGen confGen = nullptr,
           Simulation::QuorumSetAdjuster qSetAdjust = nullptr);

        static Simulation::pointer
    core(int nNodes, double quorumThresoldFraction, Simulation::Mode mode,
         Hash const& networkID, Simulation::ConfigGen confGen = nullptr,
         Simulation::QuorumSetAdjuster qSetAdjust = nullptr);

        static Simulation::pointer
    cycle(int nNodes, double quorumThresoldFraction, Simulation::Mode mode,
          Hash const& networkID, Simulation::ConfigGen confGen = nullptr,
          Simulation::QuorumSetAdjuster qSetAdjust = nullptr);

        static Simulation::pointer
    branchedcycle(int nNodes, double quorumThresoldFraction,
                  Simulation::Mode mode, Hash const& networkID,
                  Simulation::ConfigGen confGen = nullptr,
                  Simulation::QuorumSetAdjuster qSetAdjust = nullptr);

        static Simulation::pointer
    separate(int nNodes, double quorumThresoldFraction, Simulation::Mode mode,
             Hash const& networkID, Simulation::ConfigGen confGen = nullptr,
             Simulation::QuorumSetAdjuster qSetAdjust = nullptr);

            static Simulation::pointer hierarchicalQuorum(
        int nBranches, Simulation::Mode mode, Hash const& networkID,
        Simulation::ConfigGen confGen = nullptr, int connectionsToCore = 1,
        Simulation::QuorumSetAdjuster qSetAdjust = nullptr);

                    static Simulation::pointer hierarchicalQuorumSimplified(
        int coreSize, int nbOuterNodes, Simulation::Mode mode,
        Hash const& networkID, Simulation::ConfigGen confGen = nullptr,
        int connectionsToCore = 1,
        Simulation::QuorumSetAdjuster qSetAdjust = nullptr);

        static Simulation::pointer
    customA(Simulation::Mode mode, Hash const& networkID,
            Simulation::ConfigGen confGen = nullptr, int connections = 1,
            Simulation::QuorumSetAdjuster qSetAdjust = nullptr);
};
}
