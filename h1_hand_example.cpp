/**
 * @file inspire_hand.cpp
 * @brief This is an example of how to control the Unitree H1 (Inspire) Hand using unitree_sdk2.
 */

// Inspire Hand Topic IDL Types
#include <unitree/idl/go2/MotorCmds_.hpp>
#include <unitree/idl/go2/MotorStates_.hpp>
// DDS Channel
#include <unitree/robot/channel/channel_publisher.hpp>
#include <unitree/robot/channel/channel_subscriber.hpp>
#include <unitree/common/thread/thread.hpp>

#include <eigen3/Eigen/Dense>
#include <unordered_map>

/**
 * @brief Unitree H1 Hand Controller
 * The user can subscribe to "rt/inspire/state" to get the current state of the hand and publish to "rt/inspire/cmd" to control the hand.
 * 
 *                  IDL Types
 * user ---(unitree_go::msg::dds_::MotorCmds_)---> "rt/inspire/cmd"
 * user <--(unitree_go::msg::dds_::MotorStates_)-- "rt/inspire/state"
 * 
 * @attention Currently the hand only supports position control, which means only the `q` field in idl is used.
 */
class H1HandController
{
public:
    H1HandController()
    {
        this->InitDDS_();
    }

    /**
     * @brief Control the hand to a specific label
     */
    void ctrl(std::string label)
    {
        if(labels.find(label) != labels.end())
        {
            this->ctrl(labels[label], labels[label]);
        }
        else
        {
            std::cout << "Invalid label: " << label << std::endl;
        }
    }

    /**
     * @brief Move the fingers to the specified angles
     * 
     * @note The angles should be in the range [0, 1]
     *       0: close  1: open
     */
    void ctrl(
        const Eigen::Matrix<float, 6, 1>& right_angles, 
        const Eigen::Matrix<float, 6, 1>& left_angles)
    {
        for(size_t i(0); i<6; i++)
        {
            cmd.cmds()[i].q() = right_angles(i);
            cmd.cmds()[i+6].q() = left_angles(i);
        }
        handcmd->Write(cmd);
    }

    /**
     * @brief Get the right hand angles
     * 
     * Joint order: [pinky, ring, middle, index, thumb_bend, thumb_rotation]
     */
    Eigen::Matrix<float, 6, 1> getRightQ()
    {
        std::lock_guard<std::mutex> lock(mtx);
        Eigen::Matrix<float, 6, 1> q;
        for(size_t i(0); i<6; i++)
        {
            q(i) = state.states()[i].q();
        }
        return q;
    }

    /**
     * @brief Get the left hand angles
     * 
     * Joint order: [pinky, ring, middle, index, thumb_bend, thumb_rotation]
     */
    Eigen::Matrix<float, 6, 1> getLeftQ()
    {
        std::lock_guard<std::mutex> lock(mtx);
        Eigen::Matrix<float, 6, 1> q;
        for(size_t i(0); i<6; i++)
        {
            q(i) = state.states()[i+6].q();
        }
        return q;
    }

    unitree_go::msg::dds_::MotorCmds_ cmd;
    unitree_go::msg::dds_::MotorStates_ state;
private:
    void InitDDS_()
    {
        handcmd = std::make_shared<unitree::robot::ChannelPublisher<unitree_go::msg::dds_::MotorCmds_>>(
            "rt/inspire/cmd");
        handcmd->InitChannel();
        cmd.cmds().resize(12);
        handstate = std::make_shared<unitree::robot::ChannelSubscriber<unitree_go::msg::dds_::MotorStates_>>(
            "rt/inspire/state");
        handstate->InitChannel([this](const void *message){
            std::lock_guard<std::mutex> lock(mtx);
            state = *(unitree_go::msg::dds_::MotorStates_*)message;
        });
        state.states().resize(12);
    }

    // DDS parameters
    std::mutex mtx;
    unitree::robot::ChannelPublisherPtr<unitree_go::msg::dds_::MotorCmds_> handcmd;
    unitree::robot::ChannelSubscriberPtr<unitree_go::msg::dds_::MotorStates_> handstate;

    // Saved labels
    std::unordered_map<std::string, Eigen::Matrix<float, 6, 1>> labels = {
        {"open",   Eigen::Matrix<float, 6, 1>::Ones()},
        {"close",  Eigen::Matrix<float, 6, 1>::Zero()},
        {"half",   Eigen::Matrix<float, 6, 1>::Constant(0.5)},
    };
};

/**
 * Main Function
 */
 
void PeaceSign(auto h1hand)
{	
	//float pinky, ring, middle, index, thumb, thumb_r = 0;
	Eigen::Matrix<float, 6, 1> RightHand = Eigen::Matrix<float, 6, 1>::Constant(1);	
	Eigen::Matrix<float, 6, 1> LeftHand = Eigen::Matrix<float, 6, 1>::Constant(1);
	
	h1hand -> ctrl(RightHand, LeftHand); // Starts from Open hand
	usleep(100000);
	
	RightHand << 0,0,1,1,0,0; //PINKY, RING, MIDDLE, INDEX, THUMB, THUMB ROTATION
	LeftHand << 0,0,1,1,0,0;  //PINKY, RING, MIDDLE, INDEX, THUMB, THUMB ROTATION
	h1hand -> ctrl(RightHand, LeftHand);
} 

void SelectLeftorRight()
{
	std::cout << "Select hand menu" << std::endl;
	std::cout << "1: Left hand\n2: Right hand" << std::endl;
	std::cout << "Input: ";
}

void SelectFingerMenu()
{
	std::cout << "Select finger menu" << std::endl;
	std::cout << "1: Pinky\n2: Ring\n3: Middle\n4: Index\n5: Thumb\n6: Thumb Rotation" << std::endl;
	std::cout << "Input: ";
}

int getSelectHandinput()
{
    int input; 
    std::cin >> input;
    
    switch(input)
    {
        case 1: std::cout << "Left hand selected.";
                break;
                
        case 2: std::cout << "Right hand selected.";
                break;
                
        case 3: std::cout << "Both hands selected.";
                break;
                
        default: std::cout << input << "is an invalid input, try again.";
                 break;
    }
    
    return input;
}

int main(int argc, char** argv)
{
    std::cout << " --- Unitree Robotics --- \n";
    std::cout << "     H1 Hand Example      \n\n";

    // Target label
    std::string label = "close"; // You change this value to other labels

    // Initialize the DDS Channel
    std::string networkInterface = argc > 1 ? argv[1] : "";
    unitree::robot::ChannelFactory::Instance()->Init(0, networkInterface);

    // Create the H1 Hand Controller
    auto h1hand = std::make_shared<H1HandController>();

    int cnt = 0;
    while (true)
    {
       /* usleep(100000);
        if(cnt++ % 10 == 0)
            label = label == "close" ? "open" : "close";
        h1hand->ctrl(label); // Control the hand
        std::cout << "-- Hand State --\n";
        std::cout << " R: " << h1hand->getRightQ().transpose() << std::endl;
        std::cout << " L: " << h1hand->getLeftQ().transpose() << std::endl;
        std::cout << "\033[3A"; // Move cursor up 3 lines*/  
        usleep(100000);
        PeaceSign(h1hand);
    }

    return 0;
}
