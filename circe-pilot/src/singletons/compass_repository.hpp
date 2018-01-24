// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_SINGLETONS_COMPASS_REPOSITORY_HPP_
#define CIRCE_PILOT_SINGLETONS_COMPASS_REPOSITORY_HPP_

#include "../compass.hpp"

namespace Circe {
namespace Pilot {

class CompassRepository {
	friend Compass;

private:
	CompassRepository();

private:
	static bool update_compass(const volatile Compass *compass) NOEXCEPT;

public:
	static boost::shared_ptr<Compass> get_compass(const CompassKey &compass_key);
	static boost::shared_ptr<Compass> open_compass(const CompassKey &compass_key);
	static void insert_compass(const boost::shared_ptr<Compass> &compass);
	static bool remove_compass(const volatile Compass *compass) NOEXCEPT;
};

}
}

#endif
