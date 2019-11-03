#include "Objects.hpp"

void serializeCommand(Command& cmd, std::string& out) {
	std::stringstream ss;
	boost::archive::text_oarchive oa(ss);
	oa << cmd;
	out = ss.str();
}

Command deserializeCommand(const std::string& input) {
	std::stringstream ss(input);

	boost::archive::text_iarchive ia(ss);
	Command result;
	ia >> result;
	return result;
}

bool serializeObject(CustomObject* pObj, std::string& out) {
	if (pObj == nullptr)
		return false;
	if (pObj->type() == CUSTOM_TYPE_1) {
		CustomObject1* pO = dynamic_cast<CustomObject1*>(pObj);
		std::stringstream ss;
		boost::archive::text_oarchive oa(ss);
		oa << *pO;
		out = ss.str();
		return true;
	}
	else if (pObj->type() == CUSTOM_TYPE_2) {
		CustomObject2* pO = dynamic_cast<CustomObject2*>(pObj);
		std::stringstream ss;
		boost::archive::text_oarchive oa(ss);
		oa << *pO;
		out = ss.str();
		return true;
	}
	return false;
}