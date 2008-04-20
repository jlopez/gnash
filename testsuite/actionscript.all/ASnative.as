// 
//   Copyright (C) 2008 Free Software Foundation, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

rcsid="$Id: ASnative.as,v 1.6 2008/04/20 17:53:58 bwy Exp $";
#include "check.as"

a = ASnative (100, 0); // escape
check_equals(a(" "), "%20");
a = ASnative (100, 1); // unescape
check_equals(a("%20"), " ");
a = ASnative (100, 2); // parseInt
check_equals(a("  566765s"), "566765");
a = ASnative (100, 3); // parseFloat
check_equals(a("8.4e6"), 8.4e6);

// Do this first to make sure ASnative is
// registered before Date class itself is loaded (Gnash loads
// on demand).
a = ASnative(103, 257);
check_equals (a(65, 1, 1, 1, 1, 1, 1), Date.UTC(65, 1, 1, 1, 1, 1, 1));

e = ASnative(103, 256); // _global.Date
xcheck_equals(e().valueOf(), Date().valueOf());
g = e;
xcheck_equals(typeOf(g().valueOf()), 'string');

xcheck_equals(e(100000).valueOf(), Date().valueOf());

g = e();
check_equals(typeOf(g.getMilliseconds), 'undefined');

f = new e(100000000); // not instantiatable
xcheck_equals(typeof(f), 'object');
check_equals(typeof(f.getMilliseconds()), 'undefined');
check_equals(typeof(f().getMilliseconds()), 'undefined');


d = new Date (123456789);

d.a = ASnative(103, 0);
check_equals (d.a(), d.getFullYear());
d.a = ASnative(103, 1);
check_equals (d.a(), d.getYear());
d.a = ASnative(103, 2);
check_equals (d.a(), d.getMonth());
d.a = ASnative(103, 3);
check_equals (d.a(), d.getDate());
d.a = ASnative(103, 4);
check_equals (d.a(), d.getDay());
d.a = ASnative(103, 5);
check_equals (d.a(), d.getHours());
d.a = ASnative(103, 6);
check_equals (d.a(), d.getMinutes());
d.a = ASnative(103, 7);
check_equals (d.a(), d.getSeconds());
d.a = ASnative(103, 8);
check_equals (d.a(), d.getMilliseconds());
d.a = ASnative(103, 16);
check_equals (d.a(), d.getTime());
d.a = ASnative(103, 18);
check_equals (d.a(), d.getTimezoneOffset());
d.a = ASnative(103, 19);
check_equals (d.a(), d.toString());
d.a = ASnative(103, 128);
check_equals (d.a(), d.getUTCFullYear());
d.a = ASnative(103, 129);
check_equals (d.a(), d.getUTCYear());
d.a = ASnative(103, 130);
check_equals (d.a(), d.getUTCMonth());
d.a = ASnative(103, 131);
check_equals (d.a(), d.getUTCDate());
d.a = ASnative(103, 132);
check_equals (d.a(), d.getUTCDay());
d.a = ASnative(103, 133);
check_equals (d.a(), d.getUTCHours());
d.a = ASnative(103, 134);
check_equals (d.a(), d.getUTCMinutes());
d.a = ASnative(103, 135);
check_equals (d.a(), d.getUTCSeconds());
d.a = ASnative(103, 136);
check_equals (d.a(), d.getUTCMilliseconds());
d.a = ASnative(103, 1);
check_equals (d.a(), d.getYear());

countVO = 0;
countTS = 0;

// ASNative Math (call valueOf)

func = {};
func.valueOf = function () {
    //trace ("valueOf()");
    countVO++;
    return 0.3;
};

func.toString = function () {
    //trace ("toString()");
    countTS++;
    return "gNaSh mUsT woRK! öÜäÄ€€";
};

a = ASnative(200, 0);
check_equals(a(func), 0.3); // abs
check_equals(a(0.3), 0.3); // abs

a = ASnative(200, 1);
check_equals(a(func, func + 1), 0.3); // min
check_equals(a(0.3, 1.3), 0.3); // min

a = ASnative(200, 2);
check_equals(a(func, func + 1), 1.3); // max
check_equals(a(0.3, 1.3), 1.3); // max

a = ASnative(200, 3);
check_equals(a(func).toString(), "0.29552020666134"); // sin
check_equals(a(0.3).toString(), "0.29552020666134"); // sin

a = ASnative(200, 4);
check_equals(a(func).toString(), "0.955336489125606"); // cos
check_equals(a(0.3).toString(), "0.955336489125606"); // cos

a = ASnative(200, 5);
check_equals(a(func, func + 1).toString(), "0.226798848053886"); // atan2
check_equals(a(0.3, 1.3).toString(), "0.226798848053886"); // atan2

a = ASnative(200, 6);
check_equals(a(func).toString(), "0.309336249609623"); // tan
a = ASnative(200, 7);
check_equals(a(func).toString(), "1.349858807576"); // exp
a = ASnative(200, 8);
check_equals(a(func).toString(), "-1.20397280432594"); // log
a = ASnative(200, 9);
check_equals(a(func).toString(), "0.547722557505166"); // sqrt
a = ASnative(200, 10);
check_equals(a(func), 0); // round
a = ASnative(200, 11);
check(a(func) >= 0 && a(func < 1)); // random
a = ASnative(200, 12);
check_equals(a(func), 0); // floor
a = ASnative(200, 13);
check_equals(a(func), 1); // ceil
a = ASnative(200, 14);
check_equals(a(func).toString(), "0.291456794477867"); // atan
a = ASnative(200, 15);
check_equals(a(func).toString(), "0.304692654015398"); // asin
a = ASnative(200, 16);
check_equals(a(func).toString(), "1.2661036727795"); // acos
a = ASnative(200, 17);
check_equals(a(func, func + 1).toString(), "0.209053590580785"); // pow
a = ASnative(200, 18);
check_equals(a(func), false); // isNan
a = ASnative(200, 19);
check_equals(a(func), true); // isFinite

xcheck_equals (countVO, 25); // calls to valueOf.
check_equals (countTS, 0); // calls to toString.

// String functions (call toString)

a = ASnative(251, 3); // String.toUpperCase
xcheck_equals(a("Hello World"), "_LEVEL0");
a = ASnative(102, 0); // SWF5 to upper
xcheck_equals(a("Hello World"), "_LEVEL0");


// SWF5 has problems with UTF-8, tested in String.as.
// No need to test here as well.

#if OUTPUT_VERSION > 5
func.a = ASnative(251, 3); // String.toUpperCase
xcheck_equals(func.a(), "GNASH MUST WORK! ÖÜÄÄ€€");

func.a = ASnative(251, 4); // String.toLowerCase
xcheck_equals(func.a(), "gnash must work! öüää€€");
#endif

func.a = ASnative(102, 0); // SWF5 to upper
xcheck_equals(func.a(), "GNASH MUST WORK! öÜäÄ€€");

func.a = ASnative(102, 1); // SWF5 to lower
xcheck_equals(func.a(), "gnash must work! öÜäÄ€€");

#if OUTPUT_VERSION > 5
xcheck_equals (countTS, 4);
#else
xcheck_equals (countTS, 2);
#endif

xcheck_equals (countVO, 25);

#if OUTPUT_VERSION > 5
check_totals(70);
#else
check_totals(68);
#endif
