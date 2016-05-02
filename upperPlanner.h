/* 
 * Copyright: (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Nguyen Dong Hai Phuong <phuong.nguyen@iit.it>
 * website: www.robotcub.org
 * author website: https://github.com/towardthesea
 * 
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
*/

#ifndef __UPPERPLANNER_H__
#define __UPPERPLANNER_H__

#include <yarp/os/Time.h>
#include <yarp/os/RateThread.h>
#include <yarp/os/Log.h>
#include <yarp/os/Mutex.h>
#include <yarp/os/LockGuard.h>

#include <yarp/os/RpcServer.h>
#include <yarp/os/RpcClient.h>
#include <yarp/os/ResourceFinder.h>
#include <yarp/os/RFModule.h>

#include <yarp/sig/Vector.h>
#include <yarp/sig/Matrix.h>

#include <yarp/math/Math.h>

#include <iCub/ctrl/math.h>
#include <iCub/ctrl/pids.h>

#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/CartesianControl.h>
#include <yarp/dev/Drivers.h>

#include <iCub/iKin/iKinFwd.h>
#include <iCub/skinDynLib/common.h>

#include <stdarg.h>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>

#include "rrts.hpp"
#include "system_single_integrator.h"
#include "singlePlanner.h"
//#include "upperPlanner_IDL.h"

using namespace std;
using namespace RRTstar;
using namespace SingleIntegrator;
using namespace yarp::sig;
using namespace yarp::dev;
using namespace yarp::math;
using namespace yarp::os;
using namespace iCub::ctrl;
using namespace iCub::iKin;


typedef Planner<State,Trajectory,System> planner_t;
typedef Vertex<State,Trajectory,System> vertex_t;

class trajOutPort : public BufferedPort<Bottle>
{
protected:
    string controlPoint;
    vector<Vector> trajectory;
    Stamp ts;
    int nDim;

public:
    void setControlPoint(const string &ctrPoint){
        controlPoint = ctrPoint;
    }

    void setTrajectory(const vector<Vector> &traject)
    {
        trajectory = traject;
        if (trajectory.size()>0)
            nDim = trajectory[0].size();
    }

    void sendTrajectory()
    {
        Bottle &outTraj = prepare();
        outTraj.clear();
        Bottle traj, viaPoint;
        traj.clear();
        viaPoint.clear();

        ts.update();

        traj.addString(controlPoint);   // Added on 21/03/2016 for sending trajectory of different control points

        traj.addInt(trajectory.size());
        traj.addInt(nDim);
        for (int j=0; j<trajectory.size(); j++)
        {
            viaPoint.clear();
            for (int i=0; i<nDim; i++)
                viaPoint.addDouble(trajectory[j][i]);
            traj.append(viaPoint);
        }
        outTraj.addList().read(traj);
        setEnvelope(ts);
        write();
    }
};

class rpcDataProcessor : public PortReader
{
    virtual bool read(ConnectionReader& connection)
    {
        Bottle in, out;
        bool ok = in.read(connection);
        if (!ok) return false;
        // process data "in", prepare "out"
        if (in.get(0).asString() == "re_plan")
        {
            printf("receive replan cmd");
        }

        ConnectionWriter *returnToSender = connection.getWriter();
        if (returnToSender!=NULL)
        {
            out.write(*returnToSender);
        }
        return true;
    }
};

class upperPlanner: public yarp::os::RFModule//, public upperPlanner_IDL
{
protected:
    /***************************************************************************/
    // EXTERNAL VARIABLES: change them from command line or through .ini file
    // Flag that manages verbosity (v=1 -> more text printed out; v=2 -> even more text):
    int     verbosity;
    // Name of the module (to change port names accordingly):
    string  robot;          // Name of the robot
    string  name;           // Name of the module
    string  part;           // Part to use
    string  part_short;
    string  running_mode;   // To set running mode as "Single" or "Batch"
    string  targetName;     // Name of the object considered as target
    int     targetID;       // ID of the object considered as target in OPC (at /memory/rpc)

    RpcServer   rpcSrvr;
    bool    replan;        // Flag to run the planner
    RpcClient   rpc2OPC;

    // Variables for Batch operation
    unsigned int countReplan;
    unsigned int maxReplan;
    bool    success;       // Flag to indicate the plan is sucessful or not (Batch Summary only)
    double  solvingTime; // Total time for each whole planner of all control points
    double  planningTime;// Deadline for planner

    // Flag to know if the torso shall be used or not
    bool    useTorso;

    bool    disableTorso;  // flag to know if the torso has to be used or not
    bool    visualizeObjectsInSim; // using yarp rpc /icubSim/world to visualize Objects, i.e. obstacles, target
    bool    visualizeObjectsInGui;
    /***************************************************************************/
    // INTERNAL VARIABLES:

    // Variable for planner
    int nDim;
    Vector workspace;       // World frame
    Vector workspaceRoot;   // Root frame

    // Best trajectory for End Effector
    vector<Vector> bestTrajEE;
    vector<Vector> bestTrajRootEE;

    // Best trajectory for Elbow
    vector<Vector> bestTrajElbow;
    vector<Vector> bestTrajRootElbow;

    // Best trajectory for Half of Elbow
    vector<Vector> bestTrajHalfElbow;
    vector<Vector> bestTrajRootHalfElbow;

    // Set of Obstacles
    vector<Vector> obsSet;
    vector<Vector> obsSetExpandedElbow; // Expanded obstacle set of the Elbow,
                                        // generated from obstacle set of End-Effector
                                        // Obstacle set of the Elbow is combination of
                                        // obsSet and obsSetExpandedElbow
    vector<Vector> obsSetExpandedElbow_fromHalf;

    vector<Vector> obsSetExpandedHalfElbow;

    // Goal
    Vector target;
//    Vector startPos;

    // Manipulator
    iCubArm *arm;
    Vector x0;   // End-Effector initial position before planning
                 // will be read through the Dynamic chain

    // Links' length
    double lShoulder, lArm, lForearm;
    unsigned int indexElbow;


    // Ports for exchange data
    Stamp ts;   // Stamp for the setEnvelope for the ports
    BufferedPort<Bottle> upperTrajectPortOut;   // Output for Trajectory of upper body

    trajOutPort EEPortOut, HalfElbowPortOut;

    Bottle cmd;
    Bottle cmdGui;          // Bottle to display object on iCubGui
    Port portToSimWorld;
    Port portToGui;
    Matrix T_world_root;    //homogenous transf. matrix expressing the rotation and translation of FoR from world (simulator) to from robot (Root) FoR
    Matrix T_root_world;    //homogenous transf. matrix expressing the rotation and translation of FoR from robot (Root) to from world (simulator) FoR

    // Driver for "classical" interfaces
    PolyDriver       ddA;
    PolyDriver       ddT;

    // "Classical" interfaces for the arm
    IEncoders            *iencsA;
    IVelocityControl2     *ivelA;
    IControlMode2         *imodA;
    IControlLimits        *ilimA;
    yarp::sig::Vector     *encsA;

    int jntsA;

    // "Classical" interfaces for the torso
    IEncoders            *iencsT;
    IVelocityControl2     *ivelT;
    IControlMode2         *imodT;
    IControlLimits        *ilimT;
    yarp::sig::Vector     *encsT;
    int jntsT;

    /**
    * Prints a message according to the verbosity level:
    * @param l is the level of verbosity: if level > verbosity, something is printed
    * @param f is the text. Please use c standard (like printf)
    */
    int printMessage(const int l, const char *f, ...) const;

public:
    // CONSTRUCTOR: rate, name, verbosity,
    //upperPlanner(int , const string & , int );
    // CONSTRUCTOR: rate, name, verbosity, regionOperating (6), regionGoal (6)
    //upperPlanner(int , const string & , int , const Vector&, const Vector&);

    // CONSTRUCTOR:
    upperPlanner();
    /************************************************************************/
    // Inherit methods
    virtual bool configure(ResourceFinder&);

    virtual bool close();

//    virtual bool attach (RpcServer&);

    virtual double getPeriod();

    virtual bool updateModule();

    virtual bool respond(const Bottle&, Bottle&);

    /************************************************************************/
    // Planner methods
//    bool re_plan(const double& );

//    bool restartPlanner(void);

    void initBatchSummary();

    void logBatchSummary();

    /**
    * Add another waypoint between two waypoint 1 and 2, to form two control points' path with the same numbers of waypoints
    * @param point1: 3D yarp Vector of waypoint 1
    * @param point2: 3D yarp Vector of waypoint 2
    * @param pointj: 3D yarp Vector of waypoint j, in the path of another control point
    * @param lengthLimb: geometric distance of 2 control points
    * @param newPoint: 3D yarp Vector of the output padded waypoint/
    */
    bool padWaypoint(const Vector &point1, const Vector &point2,
                     const Vector &pointj, const double &lengthLimb,
                     Vector &newPoint);

    void updateArmChain();

    void processRpcCommand();

    Vector getObjFromOPC_Name(const string &objectName, int &idObject);

    vector<int> getObsIDFromOPC_Name();

    bool getObsFromOPC(const int &idObject, Vector &obstacle);

    vector<Vector> expandObstacle(const vector<Vector> &traject, const vector<Vector> &obstacles,
                                  const double &lengthLimb);

    /**
    * Find the other ending point in a link, like Elbow in the Forearm when knowing position of End-Effector and another control point
    * @param oneEndPoint: 3D yarp Vector of 3D coordinate of one control point (e.g. End-effector)
    * @param halfPoint: 3D yarp Vector of 3D coordinate of another control point (e.g. middle point in the Forearm)
    * @param lengthLimb: geometric distance of 2 control points (e.g. length of Forearm)
    * Output is the D yarp Vector of the other Ending point of link (e.g. Elbow)
    */
    Vector findOtherEndPoint(const Vector &oneEndPoint, const Vector &halfPoint,
                             const double &lengthLimb);

    double distWpObs(const Vector &waypoint, const Vector &obstacle);

    double distWpWp(const Vector &wp1, const Vector &wp2);

    /**
    * Check if the new waypoint is collided with set of obstacles
    * @param waypoint: 3D yarp Vector of 3D coordinate
    * @param obstacles: std vector of 6 parameter yarp Vector, the first 3 are 3D coordinate of the center of obstacle, the last 3 are sizes of obstacle
    */
    bool collisionCheck(const Vector &waypoint, const vector<Vector> &obstacles);

    void findCenterOnSurfaces(const Vector &obstacle,
                         vector<Vector> &centers, vector<Vector> &normVectors);

    double dotProduct(const Vector &v1, const Vector &v2);

    bool intersectionSegmentPlane(const Vector &point1, const Vector &point2,
                                  const Vector &pointOnPlane, const Vector &normalVector,
                                  Vector &intersection);

    bool collisionCheckPathSegment(const Vector &point1, const Vector &point2,
                            const vector<Vector> &obstacles);

    void displayTraj(const vector<Vector> &trajectory, const string &color);

    void logTrajectory(const string &trajectFilename, const double &time);

    void sendTrajectory(void);


    void sendTrajectory(const string &ctrPoint,
                        const vector<Vector> &trajectory);

    void sendTrajectory(const string &ctrPoint,
                        BufferedPort<Bottle> trajectPortOut,
                        const vector<Vector> &trajectory);


    void convertPosFromRootToSimFoR(const Vector &pos, Vector &outPos);

    void convertPosFromSimToRootFoR(const Vector &pos, Vector &outPos);

    //*** visualizations in icub simulator
    /**
    * Creates a sphere (not affected by gravity) in the iCub simulator through the /icubSim/world port
    * @param radius
    * @param pos
    */
    void createStaticSphere(double radius, const yarp::sig::Vector &pos, const string &color);

    void createStaticBox(const yarp::sig::Vector &pos, const string &type);

    void displayWorkspaceGui();

    void createObsGui(const Vector &pos, const string &type, const int &order);

    void displayWpsGui(const vector<Vector> &trajectory, const string &ctrlPoint, const string &color);

    void createWpGui(const Vector &pos, const string &ctrlPoint, const int &order, const string &color);

    void convertObjFromSimToRootFoR(const Vector &obj, Vector &outObj);

    void convertObjFromRootToSimFoR(const Vector &obj, Vector &outObj);

    void initShowTrajGui(const string &ctrlPoint, const string &color);

    void updateTrajGui(const vector<Vector> &trajectory, const string &ctrlPoint);
};

#endif

