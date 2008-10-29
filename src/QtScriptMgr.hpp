/*
 * Stellarium
 * Copyright (C) 2007 Fabien Chereau
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _QTSCRIPTMGR_HPP_
#define _QTSCRIPTMGR_HPP_

#include <QObject>
#include <QtScript>
#include <QStringList>
#include "vecmath.h"

	
//! Provide script API for Stellarium global functions
class StelMainScriptAPI : public QObject
{
	Q_OBJECT
	Q_PROPERTY(double JDay READ getJDay WRITE setJDay)
	Q_PROPERTY(double timeSpeed READ getTimeSpeed WRITE setTimeSpeed)
					
public:
	StelMainScriptAPI(QObject *parent = 0);
	~StelMainScriptAPI();
	
// These functions will be available in scripts
public slots:
	//! Set the current date in Julian Day
	//! @param JD the Julian Date
	void setJDay(double JD);
	//! Get the current date in Julian Day
	//! @return the Julian Date
	double getJDay(void) const;

	//! set the date in ISO format, e.g. "2008-03-24T13:21:01"
	//! @param dt the date string to use
	//! @param spec "local" or "utc"
	void setDate(const QString& dt, const QString& spec="utc");
		
	//! Set time speed in JDay/sec
	//! @param ts time speed in JDay/sec
	void setTimeSpeed(double ts);
	//! Get time speed in JDay/sec
	//! @return time speed in JDay/sec
	double getTimeSpeed(void) const;
	
	//! Pauses the script for t seconds
	void wait(double t);
	
	//! Select an object by name
	//! @param name the name of the object to select (english)
	//! @param pointer whether or not to have the selection pointer enabled
	void selectObjectByName(const QString& name, bool pointer);

	//! Clear the display options, setting a "standard" view.
	//! Preset states:
	//! - natural : azimuthal mount, atmosphere, landscape, 
	//!   no lines, labels or markers
	//! - starchart : equatorial mount, constellation lines,
	//!   no landscape, atmoshere etc.  labels & markers on.
	//! @param state the name of a preset state.
	void clear(const QString& state="natural");

	//! move the current viewing direction to some specified altitude and azimuth
	//! angles may be specified in a format recognised by StelUtils::getDecAngle()
	//! @param alt the altitude angle
	//! @param azi the azimuth angle
	//! @param duration the duration of the movement in seconds
	void moveToAltAzi(const QString& alt, const QString& azi, float duration=1.);

	//! move the current viewing direction to some specified right ascension and declination
	//! angles may be specified in a format recognised by StelUtils::getDecAngle()
	//! @param ra the right ascension angle
	//! @param dec the declination angle
	//! @param duration the duration of the movement in seconds
	void moveToRaDec(const QString& ra, const QString& dec, float duration=1.);

	//! Set the observer location
	//! @param longitude the longitude in degrees. E is +ve.  
	//!        values out of the range -180 .. 180 mean that
	//!        the longitude will not be set
	//! @param latitude the longitude in degrees. N is +ve.  
	//!        values out of the range -180 .. 180 mean that
	//!        the latitude will not be set
	//! @param altitude the new altitude in meters.
	//!        values less than -1000 mean the altitude will not
	//!        be set.
	//! @param duration the time for the transition from the 
	//!        old to the new location.
	//! @param name A name for the location (which will appear
	//!        in the status bar.
	//! @param planet the English name of the new planet.
	//!        If the planet name is not known (e.g. ""), the
	//!        planet will not be set.
	void setObserverLocation(double longitude, double latitude, double altitude, double duration=1., const QString& name="", const QString& planet="");

	//! Set the location by the name of the location.
	//! @param id the location ID as it would be found in the database
	//! of locations - do a search in the Location window to see what
	//! where is.  e.g. "York, UnitedKingdom".
	//! @param duration the number of seconds to take to move location.
	void setObserverLocation(const QString id, double duration=1.);

	//! print a debugging message to the console
	void debug(const QString& s);

};
		
//! Manage scripting in Stellarium
class QtScriptMgr : public QObject
{
Q_OBJECT
		
public:
	QtScriptMgr(const QString& startupScript, QObject *parent=0);
	~QtScriptMgr();

	QStringList getScriptList(void);

	//! Find out if a script is running
	//! @return true if a script is running, else false
	bool scriptIsRunning(void);
	//! Get the ID (filename) of the currently running script
	//! @return Empty string if no script is running, else the 
	//! ID of the script which is running.
	QString runningScriptId(void);
	
public slots:
	//! Gets a single line name of the script. 
	//! @param s the file name of the script whose name is to be returned.
	//! @return text following a comment with Name: at the start.  If no 
	//! such comment is found, the file name will be returned.  If the file
	//! is not found or cannot be opened for some reason, an Empty string
	//! will be returned.
	const QString getName(const QString& s);

	//! Gets a description of the script.
	//! @param s the file name of the script whose name is to be returned.
	//! @return text following a comment with Description: at the start.
	//! The description is considered to be over when a line with no comment 
	//! is found.  If no such comment is found, QString("") is returned.
	//! If the file is not found or cannot be opened for some reason, an 
	//! Empty string will be returned.
	const QString getDescription(const QString& s);

	//! Run the script located at the given location
	//! @param fileName the location of the file containing the script.
	//! @return false if the named script could not be run, true otherwise
	bool runScript(const QString& fileName);

	//! Stops any running script.
	//! @return false if no script was running, true otherwise.
	bool stopScript(void);

private slots:
	//! Called at the end of the running thread
	void scriptEnded();

signals:
	//! Notification when a script starts running
	void scriptRunning();
	//! Notification when a script has stopped running 
	void scriptStopped();

private:
	QScriptEngine engine;
	
	//! The thread in which scripts are run
	class StelScriptThread* thread;
};

#endif // _QTSCRIPTMGR_HPP_
