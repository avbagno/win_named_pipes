#pragma once

#include <boost/log/trivial.hpp>

#define LOG_INFO BOOST_LOG_TRIVIAL(info)  << "(" << __FILE__ << ", " << __LINE__ << ") "
#define LOG_DEBUG BOOST_LOG_TRIVIAL(debug)  << "(" << __FILE__ << ", " << __LINE__ << ") "
#define LOG_WARNING BOOST_LOG_TRIVIAL(warning)  << "(" << __FILE__ << ", " << __LINE__ << ") "
#define LOG_ERROR BOOST_LOG_TRIVIAL(error)  << "(" << __FILE__ << ", " << __LINE__ << ") "