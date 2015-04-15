/*
 * Copyright (C) 2007-2015 Istituto Italiano di Tecnologia RBCS, ADVR & iCub Facility
 * Authors: Mirko Ferrati, Cheng Fang, Silvio Traversaro
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 */



#include "Objects.hh"
#include "ObjectsServerImpl.h"
#include "common.h"

#include <gazebo/physics/World.hh>
#include <gazebo/physics/PhysicsEngine.hh>
#include <gazebo/physics/Joint.hh>
#include <gazebo/physics/Link.hh>
#include <gazebo/physics/Model.hh>
#include <gazebo/physics/Collision.hh>
#include <gazebo/physics/ContactManager.hh>
#include <gazebo/math/Vector3.hh>
#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/Wrapper.h>
#include <yarp/os/Network.h>

#include <sstream>

GZ_REGISTER_MODEL_PLUGIN(gazebo::GazeboYarpObjects)

namespace gazebo {

GazeboYarpObjects::GazeboYarpObjects() : ModelPlugin(), m_network(),node(new transport::Node)
{
}

GazeboYarpObjects::~GazeboYarpObjects()
{
}

void GazeboYarpObjects::Load(physics::ModelPtr _parent, sdf::ElementPtr _sdf)
{
    gazeboYarpObjectsLoad(_parent,_sdf);
}

void GazeboYarpObjects::gazeboYarpObjectsLoad(physics::ModelPtr _parent, sdf::ElementPtr _sdf)
{
    m_model = _parent;
    m_world = _parent->GetWorld();

    m_portName = "/world";

    m_rpcPort = new yarp::os::Port();
    if (!m_rpcPort) {
        std::cerr << "GazeboYarpObjects: Failed to create rpc port." << std::endl;
        cleanup();
        return;
    }

    m_clockServer = new ObjectsServerImpl(*this);
    if (!m_clockServer) {
        std::cerr << "GazeboYarpObjects: Could not create Objects Server." << std::endl;
        cleanup();
        return;
    }

    if (!m_clockServer->yarp().attachAsServer(*m_rpcPort)) {
        std::cerr << "GazeboYarpObjects: Failed to attach Objects Server to RPC port." << std::endl;
        cleanup();
        return;
    }

    if (!m_rpcPort->open(m_portName)) {
        std::cerr << "GazeboYarpObjects: Failed to open rpc port " << (m_portName) << std::endl;
        cleanup();
        return;
    }
    this->m_sdf=_sdf;
    node->Init();
//     createHandle();
}


bool gazebo::GazeboYarpObjects::createHandle(std::string link_name)
{
    if (m_world->GetModel("object_handle_"+link_name))
    {
        std::cout<<"I was asked to create an Handle for "<<link_name<<" which already has an handle, it should be ok to just do nothing and hope that the handle is free to be used"<<std::endl;
        return true;
    }
    sdf::SDF handleSDF;
    std::stringstream ss;
    ss << "<sdf version ='1.4'>\
    <model name ='object_handle_"<<link_name<<"'>\
        <pose>0 0 0 0 0 0</pose>\
        <link name ='object_handle_link'>\
            <pose>0 0 0 0 0 0</pose>\
            <collision name='collision'>\
                <geometry>\
                    <box>\
                        <size>0.00001 0.00001 0.00001</size>\
                    </box>\
                </geometry>\
            </collision>\
            <visual name='marker'>\
            <geometry>\
            <box>\
            <size>0.02 0.02 0.02</size>\
            </box>\
            </geometry>\
            <material>\
            <script>\
            <name>Gazebo/Blue</name>\
            <uri>__default__</uri>\
            </script>\
            </material>\
            </visual>\
            <inertial>\
                <mass>0.00001</mass>\
                <inertia>\
                    <ixx>0.00001</ixx>\
                    <ixy>0</ixy>\
                    <ixz>0</ixz>\
                    <iyy>0.00001</iyy>\
                    <iyz>0</iyz>\
                    <izz>0.00001</izz>\
                </inertia>\
            </inertial>\
        </link>\
    </model>\
    </sdf>";
    std::string handleSDF_str = ss.str();
    handleSDF.SetFromString(handleSDF_str);
    m_world->InsertModelSDF(handleSDF);
    
    return true;
}

void GazeboYarpObjects::cleanup()
{
    if (m_rpcPort) {
        m_rpcPort->close();
        delete m_rpcPort; m_rpcPort = 0;
    }

    if (m_clockServer) {
        delete m_clockServer; m_clockServer = 0;
    }
}

bool hasEnding (std::string const &fullString, std::string const &ending)
{
//     std::cout<<fullString<<" "<<ending<<std::endl;
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

gazebo::physics::LinkPtr getLinkInModel(gazebo::physics::ModelPtr model, std::string link_name)
{
    gazebo::physics::Link_V model_links = model->GetLinks();
    for(int i=0; i < model_links.size(); i++ ) {
        std::string candidate_link = model_links[i]->GetScopedName();
        
//         std::cout<<candidate_link<<std::endl;
        if( hasEnding(candidate_link,"::"+link_name) ) {
            return model_links[i];
        }
    }

    return gazebo::physics::LinkPtr();
}

gazebo::physics::LinkPtr getClosestLinkInModel(gazebo::physics::ModelPtr model, gazebo::math::Pose pose)
{
    int index=-1;
    double min_norm=100000;
    gazebo::physics::Link_V model_links = model->GetLinks();
    for(int i=0; i < model_links.size(); i++ ) 
    {
        std::string candidate_link = model_links[i]->GetScopedName();
        double norm = pose.pos.Distance(model_links[i]->GetWorldPose().pos);
        std::cout<<candidate_link<<" "<<norm<<std::endl;
        if (/*norm<0.25 && */norm<min_norm)
        {
            min_norm = norm;
            index = i;
            //         std::string candidate_link = model_links[i]->GetScopedName();
            //         std::cout<<candidate_link<<std::endl;
        }
    }
    if (index>-1)
        return model_links[index];
    else
        return gazebo::physics::LinkPtr();
}

bool gazebo::GazeboYarpObjects::attach(const std::string& link_name, const std::string& object_name)
{
    bool found=false;
    for (int i = 0; i < grasps.size(); i++)
    {
        if (grasps[i].object_name==object_name && grasps[i].link_name==link_name)
            found=true;
    }
    if (found)
    {
        std::cout<<"object" << object_name <<" is already attached to link "<<link_name<<std::endl;
        return false;
    }
    grasp_info result_grasp;
    physics::ModelPtr object_model = m_world->GetModel(object_name);

    if( !object_model )
    {
        std::cout<<"could not get the model for "<<object_name<<"!!"<<std::endl;
        return false;
    }
    result_grasp.object_name=object_name;
    gazebo::physics::LinkPtr parent_link = getLinkInModel(m_model,link_name);

    if (!parent_link)
    {
        std::cout<<"could not get the links for "<<parent_link<<std::endl;
        return false;
    }
    result_grasp.link_name=link_name;
    createHandle(link_name);
    result_grasp.handle_name= std::string("object_handle_").append(link_name);
    gazebo::physics::Collision_V collisions=  parent_link->GetCollisions();
    for (int j=0;j<collisions.size();j++)
    {
        result_grasp.collisions_str.push_back(collisions[j]->GetScopedName());
        std::cout<<collisions[j]->GetScopedName()<<std::endl;
    }
    
    //     collisions_str.push_back("bigman::LWrMot3::LWrMot3_collision_LWrMot3_1");
    physics::ContactManager *mgr =
    m_world->GetPhysicsEngine()->GetContactManager();
    std::string topic = mgr->CreateFilter(std::string("objects_topic_").append(link_name), result_grasp.collisions_str);
    std::cout<<topic<<std::endl;
    result_grasp.filter_name=std::string("objects_topic_").append(link_name);
    result_grasp.contactSub = node->Subscribe(topic, &GazeboYarpObjects::OnContacts, this);
    result_grasp.grasped=false;
    grasps.push_back(result_grasp);
    return true;
}

bool GazeboYarpObjects::attach_impl(std::string link_name,std::string object_name, math::Pose handle_pose, math::Vector3 normal)
{
    int index=-1;
    for (int i=0;i<grasps.size();i++)
    {
        if (grasps[i].object_name==object_name && grasps[i].link_name==link_name)
        {
            index=i;
        }
    }
    assert(index!=-1);
    grasp_info& current_grasp=grasps[index];
    std::cout<<"grasp info "<<current_grasp.filter_name<<" "<<current_grasp.grasped<<" "<<current_grasp.handle_name<<" "<<current_grasp.link_name<<" "<<current_grasp.object_name<<std::endl;
    physics::LinkPtr parent_link = getLinkInModel(m_model,current_grasp.link_name);
    
    if( !parent_link ) {
        std::cout<<"could not get the links for "<<current_grasp.link_name<<std::endl;
        return false;
    }
    math::Pose parent_link_pose = parent_link->GetWorldPose();
    physics::ModelPtr object_model = m_world->GetModel(current_grasp.object_name);
    
    if( !object_model )
    {
        std::cout<<"could not get the model for "<<current_grasp.object_name<<"!!"<<std::endl;
        return false;
    }

    
//     if (current_grasp.link_name=="LWrMot3")
//     {
//         physics::LinkPtr link = getLinkInModel(m_model,"LWrMot2");
//         if (link)
//             link->SetCollideMode("none");
//     }
//     if (current_grasp.link_name=="RWrMot3")
//     {
//         physics::LinkPtr link = getLinkInModel(m_model,"RWrMot2");
//         if (link)
//             link->SetCollideMode("none");
//     }
    parent_link->SetCollideMode("none");

    physics::ModelPtr object_handle_model = m_world->GetModel(current_grasp.handle_name);
    physics::LinkPtr object_handle_link = getLinkInModel(object_handle_model,"object_handle_link");
    object_handle_model->SetWorldPose(handle_pose);
//     object_handle_link->SetWorldPose(handle_pose);

    physics::JointPtr object_joint;
    object_joint = m_world->GetPhysicsEngine()->CreateJoint("revolute", object_model);
    if( !object_joint ) {
        std::cout<<"could not create joint for the object!!"<<std::endl;
        return false;
    }
    current_grasp.joint_attached_to_object=object_joint;
    
    physics::LinkPtr object_link = getClosestLinkInModel(object_model,parent_link_pose);
    object_joint->SetName(object_name+"_attached_handle_joint");
    object_joint->Load(object_link, object_handle_link, math::Pose());
    object_joint->Attach(object_link, object_handle_link);
    object_joint->SetHighStop(0, 0);
    object_joint->SetLowStop(0, 0);
//     object_link->SetWorldPose(handle_pose);//TODO

    physics::JointPtr joint;
    joint = m_world->GetPhysicsEngine()->CreateJoint("revolute", m_model);
    if( !joint ) {
        std::cout<<"could not create joint!!"<<std::endl;
        return false;
    }
    math::Pose offset(0,0,-0.15,0,0,0);
    math::Pose offset_pose=parent_link_pose*offset;
//     object_link->SetWorldPose(offset_pose);//TODO
    joint->SetName(object_name+"_attached_joint");
    joint->Load(parent_link, object_handle_link, math::Pose());
    joint->Attach(parent_link, object_handle_link);
    joint->SetHighStop(0, 0);
    joint->SetLowStop(0, 0);

    current_grasp.joint_attached_to_model=joint;

    //joint->SetParam("cfm", 0, 0);
    std::cout<<"Grasped!"<<std::endl;
    current_grasp.grasped=true;
    return true;
}

void GazeboYarpObjects::OnContacts(ConstContactsPtr& iter)
{
    std::vector<std::string>::iterator collIter, objectIter;
    std::string collision1, collision2, object1, object2;
    for (int j=0; j < (iter)->contact_size(); ++j)
    {
        collision1 = (iter)->contact(j).collision1();
        collision2 = (iter)->contact(j).collision2();
        object1 = (iter)->contact(j).collision1();
        object2 = (iter)->contact(j).collision2();
        if (!object1.find_first_of("::"))
        {
            std::cout<<"error in searching for object name in "<<object1<<std::endl;
            return;
        }
        if (!object2.find_first_of("::"))
        {
            std::cout<<"error in searching for object name in "<<object2<<std::endl;
            return;
        }
        object1=object1.substr(0,object1.find_first_of("::"));
        object2=object2.substr(0,object2.find_first_of("::"));
        bool found=false;
        for (int i = 0; i < grasps.size(); i++)
        {
            std::string object, collision;
            if (grasps[i].grasped) continue;
            if (object1==grasps[i].object_name)
            {
                object=object1;
                collision=collision2;
            }
            else if (object2==grasps[i].object_name)
            {
                object=object2;
                collision=collision1;
            }
            else continue;
//             if (object == grasps[i].object_name)
            {
                for (int k=0;k<grasps[i].collisions_str.size();k++)
                {
                    if (collision == grasps[i].collisions_str[k])
                    {
                        collision = grasps[i].link_name;
                        found=true;
                        break;
                        //Ok like this, do what is needed
                    }
                }
            }
            if (found)
            {
                //         std::cout<<(iter)->contact(j).position(0).x()<<" "<<(iter)->contact(j).position(0).y()<<" "<<(iter)->contact(j).position(0).z()<<std::endl;
                gazebo::math::Pose touch_point;
                touch_point.pos.x=(iter)->contact(j).position(0).x();
                touch_point.pos.y=(iter)->contact(j).position(0).y();
                touch_point.pos.z=(iter)->contact(j).position(0).z();
                math::Vector3 normal;
                normal.x=(iter)->contact(j).position(0).x();
                normal.y=(iter)->contact(j).position(0).y();
                normal.z=(iter)->contact(j).position(0).z();
                attach_impl(collision,object,touch_point,normal);
                return;
            }
        }
    }
    return;
}


bool gazebo::GazeboYarpObjects::detach(const std::string& link_name, const std::string& object_name )
{
    int index=-1;
    for (int i=0;i<grasps.size();i++)
    {
        if (grasps[i].object_name==object_name && grasps[i].link_name==link_name)
        {
            index=i;
        }
    }
    if (index==-1)
    {
        std::cout<<link_name<<" is not attached to "<<object_name<<std::endl;
        return false;
    }
    grasp_info& current_grasp=grasps[index];

    physics::ModelPtr object_model = m_world->GetModel(current_grasp.object_name);
    if( !object_model )
    {
        std::cout<<"could not get the model for "<<current_grasp.object_name<<"!!"<<std::endl;
        return false;
    }

    if (current_grasp.grasped)
    {
        current_grasp.joint_attached_to_model->Detach();
        current_grasp.joint_attached_to_object->Detach();
    }
//     if (current_grasp.link_name=="LWrMot3")
//     {
//         physics::LinkPtr link = getLinkInModel(m_model,"LWrMot2");
//         if (link)
//             link->SetCollideMode("all");
//     }
//     if (current_grasp.link_name=="RWrMot3")
//     {
//         physics::LinkPtr link = getLinkInModel(m_model,"RWrMot2");
//         if (link)
//             link->SetCollideMode("all");
//     }
    physics::LinkPtr parent_link = getLinkInModel(m_model,current_grasp.link_name);
    
    if( !parent_link ) {
        std::cout<<"could not get the links for "<<current_grasp.link_name<<std::endl;
    }
    else
        parent_link->SetCollideMode("all");
    physics::ContactManager *mgr =
    m_world->GetPhysicsEngine()->GetContactManager();
    current_grasp.contactSub->Unsubscribe();
    mgr->RemoveFilter(current_grasp.filter_name);
    grasps.erase(grasps.begin()+index);
    return true;
}


}