/******************************************************************************
 *   Copyright (C) 2014  A.J. Admiraal                                        *
 *   code@admiraal.dds.nl                                                     *
 *                                                                            *
 *   This program is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License version 3 as        *
 *   published by the Free Software Foundation.                               *
 *                                                                            *
 *   This program is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *   GNU General Public License for more details.                             *
 *                                                                            *
 *   You should have received a copy of the GNU General Public License        *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 ******************************************************************************/

#ifndef HTML_WELCOMEPAGE_H
#define HTML_WELCOMEPAGE_H

#include "mainpage.h"
#include <memory>
#include <ostream>

class settings;
class test;

namespace html {

class welcomepage
{
public:
    welcomepage(
            class mainpage &,
            class pupnp::upnp &,
            class settings &,
            const std::unique_ptr<class test> &,
            const std::function<void()> &apply);

    ~welcomepage();

private:
    void render_headers(const struct pupnp::upnp::request &, std::ostream &);
    int render_page(const struct pupnp::upnp::request &, std::ostream &);

    void render_main_page(std::ostream &out);
    void render_setup_name(const struct pupnp::upnp::request &, std::ostream &out);
    void render_setup_network(const struct pupnp::upnp::request &, std::ostream &out);
    void render_setup_codecs(const struct pupnp::upnp::request &, std::ostream &out);
    void render_setup_high_definition(const struct pupnp::upnp::request &, std::ostream &out);
    void render_setup_finish(const struct pupnp::upnp::request &, std::ostream &out);

private:
    static std::string device_type;
    static std::string client_name;

    class mainpage &mainpage;
    class pupnp::upnp &upnp;
    class settings &settings;
    const std::unique_ptr<class test> &test;
    const std::function<void()> apply;
    bool applying;
};

} // End of namespace

#endif