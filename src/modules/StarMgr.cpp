/*
 * Stellarium
 * Copyright (C) 2002 Fabien Chereau
 *
 * The big star catalogue extension to Stellarium:
 * Author and Copyright: Johannes Gajdosik, 2006
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

// class used to manage groups of Stars

#include <config.h>
#include <QTextStream>
#include <QFile>
#include <QSettings>
#include <QString>
#include <QRegExp>
#include <QDebug>

#include "Projector.hpp"
#include "StarMgr.hpp"
#include "StelObject.hpp"
#include "STexture.hpp"
#include "Navigator.hpp"
#include "StelUtils.hpp"
#include "ToneReproducer.hpp"
#include "Translator.hpp"
#include "GeodesicGrid.hpp"
#include "LoadingBar.hpp"
#include "Translator.hpp"
#include "StelApp.hpp"
#include "StelTextureMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelFontMgr.hpp"
#include "StelLocaleMgr.hpp"
#include "StelSkyCultureMgr.hpp"
#include "StelFileMgr.hpp"
#include "bytes.h"
#include "StelModuleMgr.hpp"
#include "StelCore.hpp"
#include "StelIniParser.hpp"

#include "ZoneArray.hpp"
#include "StringArray.hpp"

#include <list>

#include <errno.h>
#include <unistd.h>

using namespace BigStarCatalogExtension;

static StringArray spectral_array;
static StringArray component_array;

QString StarMgr::convertToSpectralType(int index) {
  if (index < 0 || index >= spectral_array.getSize()) {
    qDebug() << "convertToSpectralType: bad index: " << index
             << ", max: " << spectral_array.getSize();
    return "";
  }
  return spectral_array[index];
}

QString StarMgr::convertToComponentIds(int index) {
  if (index < 0 || index >= component_array.getSize()) {
    qDebug() << "convertToComponentIds: bad index: " << index
             << ", max: " << component_array.getSize();
    return "";
  }
  return component_array[index];
}





void StarMgr::initTriangle(int lev,int index,
                           const Vec3d &c0,
                           const Vec3d &c1,
                           const Vec3d &c2) {
  ZoneArrayMap::const_iterator it(zone_arrays.find(lev));
  if (it!=zone_arrays.end()) it->second->initTriangle(index,c0,c1,c2);
}


StarMgr::StarMgr(void) :
    hip_index(new HipIndexStruct[NR_OF_HIP+1]),
	fontSize(13.),
    starFont(0)
{
	setObjectName("StarMgr");
  if (hip_index == 0) {
    qWarning() << "ERROR: StarMgr::StarMgr: no memory";
    exit(1);
  }
  max_geodesic_grid_level = -1;
  last_max_search_level = -1;
}

/*************************************************************************
 Reimplementation of the getCallOrder method
*************************************************************************/
double StarMgr::getCallOrder(StelModuleActionName actionName) const
{
	if (actionName==StelModule::ACTION_DRAW)
		return StelApp::getInstance().getModuleMgr().getModule("ConstellationMgr")->getCallOrder(actionName)+10;
	return 0;
}


StarMgr::~StarMgr(void) {
  ZoneArrayMap::iterator it(zone_arrays.end());
  while (it!=zone_arrays.begin()) {
    --it;
    delete it->second;
    it->second = NULL;
  }
  zone_arrays.clear();
  if (hip_index) delete[] hip_index;
}

bool StarMgr::flagSciNames = true;
double StarMgr::current_JDay = 0;
map<int,QString> StarMgr::common_names_map;
map<int,QString> StarMgr::common_names_map_i18n;
map<QString,int> StarMgr::common_names_index;
map<QString,int> StarMgr::common_names_index_i18n;

map<int,QString> StarMgr::sci_names_map_i18n;
map<QString,int> StarMgr::sci_names_index_i18n;

QString StarMgr::getCommonName(int hip) {
  map<int,QString>::const_iterator it(common_names_map_i18n.find(hip));
  if (it!=common_names_map_i18n.end()) return it->second;
  return "";
}

QString StarMgr::getSciName(int hip) {
  map<int,QString>::const_iterator it(sci_names_map_i18n.find(hip));
  if (it!=sci_names_map_i18n.end()) return it->second;
  return "";
}




void StarMgr::init() {
	QSettings* conf = StelApp::getInstance().getSettings();
	assert(conf);

	load_data();
	double fontSize = 12;
	starFont = &StelApp::getInstance().getFontManager().getStandardFont(StelApp::getInstance().getLocaleMgr().getSkyLanguage(), fontSize);

	setFlagStars(conf->value("astro/flag_stars", true).toBool());
	setFlagNames(conf->value("astro/flag_star_name",true).toBool());
	setMaxMagName(conf->value("stars/max_mag_star_name",1.5).toDouble());
	
	StelApp::getInstance().getStelObjectMgr().registerStelObjectMgr(this);

	StelApp::getInstance().getTextureManager().setDefaultParams();
	texPointer = StelApp::getInstance().getTextureManager().createTexture("pointeur2.png");   // Load pointer texture
}

void StarMgr::setGrid(GeodesicGrid* geodesic_grid) {
  geodesic_grid->visitTriangles(max_geodesic_grid_level,initTriangleFunc,this);
  for (ZoneArrayMap::const_iterator it(zone_arrays.begin());
       it!=zone_arrays.end();it++) {
    it->second->scaleAxis();
  }
}


void StarMgr::drawPointer(const Projector* prj, const Navigator * nav)
{
	const std::vector<StelObjectP> newSelected = StelApp::getInstance().getStelObjectMgr().getSelectedObject("Star");
	if (!newSelected.empty())
	{
		const StelObjectP obj = newSelected[0];
		Vec3d pos=obj->getObsJ2000Pos(nav);
		Vec3d screenpos;
		// Compute 2D pos and return if outside screen
		if (!prj->project(pos, screenpos)) return;
	
		glColor3fv(obj->getInfoColor());
		float diameter = 26.f;
		texPointer->bind();
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Normal transparency mode
        prj->drawSprite2dMode(screenpos[0], screenpos[1], diameter, StelApp::getInstance().getTotalRunTime()*40.);
	}
}

void StarMgr::setColorScheme(const QSettings* conf, const QString& section)
{
	// Load colors from config file
	QString defaultColor = conf->value(section+"/default_color").toString();
	setLabelColor(StelUtils::str_to_vec3f(conf->value(section+"/star_label_color", defaultColor).toString()));
}

/***************************************************************************
 Load star catalogue data from files.
 If a file is not found, it will be skipped.
***************************************************************************/
void StarMgr::load_data()
{
	LoadingBar& lb = *StelApp::getInstance().getLoadingBar();
			
	// Please do not init twice:
	assert(max_geodesic_grid_level < 0);

	qDebug() << "Loading star data ...";

	QString iniFile;
	try
	{
		iniFile = StelApp::getInstance().getFileMgr().findFile("stars/default/stars.ini");
	}
	catch (exception& e)
	{
		qWarning() << "ERROR - could not find stars/default/stars.ini : " << e.what() << iniFile;
		return;
	}

	QSettings conf(iniFile, StelIniFormat);
	if (conf.status() != QSettings::NoError)
	{
		qWarning() << "ERROR while parsing " << iniFile;
		return;
	}
				         
	for (int i=0; i<100; i++)
	{
		//sprintf(key_name,"cat_file_name_%02d",i);
		QString keyName = QString("cat_file_name_%1").arg(i,2,10,QChar('0'));
		const QString cat_file_name = conf.value(QString("stars/")+keyName,"").toString();
		if (!cat_file_name.isEmpty()) {
			lb.SetMessage(q_("Loading catalog %1").arg(cat_file_name));
			ZoneArray *const z = ZoneArray::create(*this,cat_file_name,lb);
			if (z)
			{
				if (max_geodesic_grid_level < z->level)
				{
					max_geodesic_grid_level = z->level;
				}
				ZoneArray *&pos(zone_arrays[z->level]);
				if (pos)
				{
					qDebug() << cat_file_name << ", " << z->level << ": duplicate level";
					delete z;
				}
				else
				{
					pos = z;
				}
			}
		}
	}

	for (int i=0; i<=NR_OF_HIP; i++)
	{
		hip_index[i].a = 0;
		hip_index[i].z = 0;
		hip_index[i].s = 0;
	}
	for (ZoneArrayMap::const_iterator it(zone_arrays.begin());
	                it != zone_arrays.end();it++)
	{
		it->second->updateHipIndex(hip_index);
	}

	const QString cat_hip_sp_file_name = conf.value("stars/cat_hip_sp_file_name","").toString();
	if (cat_hip_sp_file_name.isEmpty())
	{
		qWarning() << "ERROR: stars:cat_hip_sp_file_name not found";
	}
	else
	{
		try
		{
			spectral_array.initFromFile(StelApp::getInstance().getFileMgr().findFile("stars/default/" + cat_hip_sp_file_name));
		}
		catch (exception& e)
		{
			qWarning() << "ERROR while loading data from "
			           << ("stars/default/" + cat_hip_sp_file_name)
			           << ": " << e.what();
		}
	}

	const QString cat_hip_cids_file_name = conf.value("stars/cat_hip_cids_file_name","").toString();
	if (cat_hip_cids_file_name.isEmpty())
	{
		qWarning() << "ERROR: stars:cat_hip_cids_file_name not found";
	}
	else
	{
		try
		{
			component_array.initFromFile(StelApp::getInstance().getFileMgr()
			        .findFile("stars/default/" + cat_hip_cids_file_name));
		}
		catch (exception& e)
		{
			qWarning() << "ERROR while loading data from "
			           << ("stars/default/" + cat_hip_cids_file_name)
			           << ": " << e.what();
		}
	}

	last_max_search_level = max_geodesic_grid_level;
	qDebug() << "Finished loading star catalogue data, max_geodesic_level: " << max_geodesic_grid_level;
}

// Load common names from file 
int StarMgr::load_common_names(const QString& commonNameFile) {
	common_names_map.clear();
	common_names_map_i18n.clear();
	common_names_index.clear();
	common_names_index_i18n.clear();

	qDebug() << "Loading star names from" << commonNameFile;
	QFile cnFile(commonNameFile);
	if (!cnFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		qWarning() << "WARNING - could not open" << commonNameFile;
		return 0;
	}

	int readOk=0;
	int totalRecords=0;
	int lineNumber=0;
	QString record;
	QRegExp commentRx("^(\\s*#.*|\\s*)$");
	// record structure is delimited with a | character.  We will
	// use a QRegExp to extract the fields. with whitespace padding permitted 
	// (i.e. it will be stripped automatically) Example record strings:
	// " 10819|c_And"
	// "113726|1_And"
	QRegExp recordRx("^\\s*(\\d+)\\s*\\|(.*)\\n");

	while(!cnFile.atEnd())
	{
		record = QString::fromUtf8(cnFile.readLine());
		lineNumber++;
		if (commentRx.exactMatch(record))
			continue;

		totalRecords++;
		if (!recordRx.exactMatch(record))
		{
			qWarning() << "WARNING - parse error at line" << lineNumber << "in" << commonNameFile 
				   << " - record does not match record pattern";
			continue;
		}
		else
		{
			// The record is the right format.  Extract the fields
			bool ok;
			unsigned int hip = recordRx.capturedTexts().at(1).toUInt(&ok);
			if (!ok)
			{
				qWarning() << "WARNING - parse error at line" << lineNumber << "in" << commonNameFile 
					   << " - failed to convert " << recordRx.capturedTexts().at(1) << "to a number";
				continue;
			}
			QString englishCommonName = recordRx.capturedTexts().at(2).trimmed();
			if (englishCommonName.isEmpty())
			{
				qWarning() << "WARNING - parse error at line" << lineNumber << "in" << commonNameFile 
					   << " - empty name field";
				continue;
			}

			englishCommonName.replace('_', ' ');
			const QString commonNameI18n = q_(englishCommonName);
			QString commonNameI18n_cap = commonNameI18n.toUpper();

			common_names_map[hip] = englishCommonName;
			common_names_index[englishCommonName] = hip;
			common_names_map_i18n[hip] = commonNameI18n;
			common_names_index_i18n[commonNameI18n_cap] = hip;
			readOk++;
		}
	}
	cnFile.close();

	qDebug() << "Loaded" << readOk << "/" << totalRecords << "common star names";
	return 1;
}


// Load scientific names from file 
void StarMgr::load_sci_names(const QString& sciNameFile)
{
	sci_names_map_i18n.clear();
	sci_names_index_i18n.clear();

	qDebug() << "Loading star names from" << sciNameFile;
	QFile snFile(sciNameFile);
	if (!snFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		qWarning() << "WARNING - could not open" << sciNameFile;
		return;
	}

	int readOk=0;
	int totalRecords=0;
	int lineNumber=0;
	QString record;
	QRegExp commentRx("^(\\s*#.*|\\s*)$");
	// record structure is delimited with a | character.  We will
	// use a QRegExp to extract the fields. with whitespace padding permitted 
	// (i.e. it will be stripped automatically) Example record strings:
	// " 10819|c_And"
	// "113726|1_And"
	QRegExp recordRx("^\\s*(\\d+)\\s*\\|(.*)\\n");

	while(!snFile.atEnd())
	{
		record = QString::fromUtf8(snFile.readLine());
		lineNumber++;
		if (commentRx.exactMatch(record))
			continue;

		totalRecords++;
		if (!recordRx.exactMatch(record))
		{
			qWarning() << "WARNING - parse error at line" << lineNumber << "in" << sciNameFile 
				   << " - record does not match record pattern";
			continue;
		}
		else
		{
			// The record is the right format.  Extract the fields
			bool ok;
			unsigned int hip = recordRx.capturedTexts().at(1).toUInt(&ok);
			if (!ok)
			{
				qWarning() << "WARNING - parse error at line" << lineNumber << "in" << sciNameFile 
					   << " - failed to convert " << recordRx.capturedTexts().at(1) << "to a number";
				continue;
			}

			// Don't set the sci name if it's already set
			if (sci_names_map_i18n.find(hip)!=sci_names_map_i18n.end())
			{
				//qWarning() << "WARNING - duplicate name for HP" << hip << "at line" 
				//           << lineNumber << "in" << sciNameFile << "SKIPPING";
				continue;
			}

			QString sci_name_i18n = recordRx.capturedTexts().at(2).trimmed();
			if (sci_name_i18n.isEmpty())
			{
				qWarning() << "WARNING - parse error at line" << lineNumber << "in" << sciNameFile 
					   << " - empty name field";
				continue;
			}

			sci_name_i18n.replace('_',' ');
			QString sci_name_i18n_cap = sci_name_i18n.toUpper();
			sci_names_map_i18n[hip] = sci_name_i18n;
			sci_names_index_i18n[sci_name_i18n_cap] = hip;
			readOk++;
		}
	}
	snFile.close();
	qDebug() << "Loaded" << readOk << "/" << totalRecords << "scientific star names";
}


int StarMgr::getMaxSearchLevel() const
{
  int rval = -1;
  for (ZoneArrayMap::const_iterator it(zone_arrays.begin());
       it!=zone_arrays.end();it++) {
    const float mag_min = 0.001f*it->second->mag_min;
    float rcmag[2];
    if (StelApp::getInstance().getCore()->getSkyDrawer()->computeRCMag(mag_min,rcmag) < 0)
		break;
    rval = it->first;
  }
  return rval;
}


// Draw all the stars
void StarMgr::draw(StelCore* core)
{
	Navigator* nav = core->getNavigation();
	Projector* prj = core->getProjection();
	SkyDrawer* skyDrawer = core->getSkyDrawer();
	
    current_JDay = nav->getJDay();

    // If stars are turned off don't waste time below
    // projecting all stars just to draw disembodied labels
    if (!starsFader.getInterstate())
		return;

	int max_search_level = getMaxSearchLevel();
	const GeodesicSearchResult* geodesic_search_result = core->getGeodesicGrid()->search(prj->unprojectViewport(),max_search_level);

    // Set temporary static variable for optimization
    const float names_brightness = names_fader.getInterstate() * starsFader.getInterstate();
    
    prj->setCurrentFrame(Projector::FRAME_J2000);

	// Prepare openGL for drawing many stars
	skyDrawer->preDrawPointSource();

    // draw all the stars of all the selected zones
    float rcmag_table[2*256];
//static int count = 0;
//count++;
	
    for (ZoneArrayMap::const_iterator it(zone_arrays.begin()); it!=zone_arrays.end();it++)
	{
		const float mag_min = 0.001f*it->second->mag_min;
		// if (maxMag < mag_min) break;
		const float k = (0.001f*it->second->mag_range)/it->second->mag_steps;
		for (int i=it->second->mag_steps-1;i>=0;i--)
		{
			const float mag = mag_min+k*i;
			if (skyDrawer->computeRCMag(mag,rcmag_table + 2*i) < 0)
			{
				if (i==0) goto exit_loop;
			}
			if (skyDrawer->getFlagPointStar())
			{
				rcmag_table[2*i+1] *= starsFader.getInterstate();
			}
			else
			{
				rcmag_table[2*i] *= starsFader.getInterstate();
			}
		}
		last_max_search_level = it->first;
	
		unsigned int max_mag_star_name = 0;
		if (names_fader.getInterstate())
		{
			int x = (int)((maxMagStarName-mag_min)/k);
			if (x > 0) max_mag_star_name = x;
		}
		int zone;
		for (GeodesicSearchInsideIterator it1(*geodesic_search_result,it->first);(zone = it1.next()) >= 0;)
		{
			it->second->draw(zone,true,rcmag_table,prj,
							max_mag_star_name,names_brightness,
							starFont);
		}
		for (GeodesicSearchBorderIterator it1(*geodesic_search_result,it->first);(zone = it1.next()) >= 0;)
		{
			it->second->draw(zone,false,rcmag_table,prj,
							max_mag_star_name,names_brightness,
							starFont);
		}
    }
    exit_loop:
	// Finish drawing many stars
	skyDrawer->postDrawPointSource();
	
	drawPointer(prj, nav);
}






// Look for a star by XYZ coords
StelObjectP StarMgr::search(Vec3d pos) const {
assert(0);
  pos.normalize();
  vector<StelObjectP > v = searchAround(pos,
                                        0.8, // just an arbitrary number
                                        NULL);
  StelObjectP nearest;
  double cos_angle_nearest = -10.0;
  for (vector<StelObjectP >::const_iterator it(v.begin());it!=v.end();it++) {
    const double c = (*it)->getObsJ2000Pos(0)*pos;
    if (c > cos_angle_nearest) {
      cos_angle_nearest = c;
      nearest = *it;
    }
  }
  return nearest;
}

// Return a stl vector containing the stars located
// inside the lim_fov circle around position v
vector<StelObjectP > StarMgr::searchAround(const Vec3d& vv,
                                           double lim_fov, // degrees
										   const StelCore* core) const {
  vector<StelObjectP > result;
  if (!getFlagStars())
  	return result;
  	
  Vec3d v(vv);
  v.normalize();
    // find any vectors h0 and h1 (length 1), so that h0*v=h1*v=h0*h1=0
  int i;
  {
    const double a0 = fabs(v[0]);
    const double a1 = fabs(v[1]);
    const double a2 = fabs(v[2]);
    if (a0 <= a1) {
      if (a0 <= a2) i = 0;
      else i = 2;
    } else {
      if (a1 <= a2) i = 1;
      else i = 2;
    }
  }
  Vec3d h0(0.0,0.0,0.0);
  h0[i] = 1.0;
  Vec3d h1 = h0 ^ v;
  h1.normalize();
  h0 = h1 ^ v;
  h0.normalize();
    // now we have h0*v=h1*v=h0*h1=0.
    // construct a region with 4 corners e0,e1,e2,e3 inside which
    // all desired stars must be:
  double f = 1.4142136 * tan(lim_fov * M_PI/180.0);
  h0 *= f;
  h1 *= f;
  Vec3d e0 = v + h0;
  Vec3d e1 = v + h1;
  Vec3d e2 = v - h0;
  Vec3d e3 = v - h1;
  f = 1.0/e0.length();
  e0 *= f;
  e1 *= f;
  e2 *= f;
  e3 *= f;
    // search the triangles
 	const GeodesicSearchResult* geodesic_search_result = core->getGeodesicGrid()->search(e3,e2,e1,e0,last_max_search_level);
    // iterate over the stars inside the triangles:
  f = cos(lim_fov * M_PI/180.);
  for (ZoneArrayMap::const_iterator it(zone_arrays.begin());
       it!=zone_arrays.end();it++) {
//qDebug() << "search inside(" << it->first << "):";
    int zone;
    for (GeodesicSearchInsideIterator it1(*geodesic_search_result,it->first);
         (zone = it1.next()) >= 0;) {
      it->second->searchAround(zone,v,f,result);
//qDebug() << " " << zone;
    }
//qDebug() << endl << "search border(" << it->first << "):";
    for (GeodesicSearchBorderIterator it1(*geodesic_search_result,it->first);
         (zone = it1.next()) >= 0;) {
      it->second->searchAround(zone,v,f,result);
//qDebug() << " " << zone;
    }
  }
  return result;
}






//! Update i18 names from english names according to passed translator.
//! The translation is done using gettext with translated strings defined in translations.h
void StarMgr::updateI18n() {
  Translator trans = StelApp::getInstance().getLocaleMgr().getSkyTranslator();
  common_names_map_i18n.clear();
  common_names_index_i18n.clear();
  for (map<int,QString>::iterator it(common_names_map.begin());
       it!=common_names_map.end();it++) {
    const int i = it->first;
    const QString t(trans.qtranslate(it->second));
    common_names_map_i18n[i] = t;
    common_names_index_i18n[t.toUpper()] = i;
  }
  starFont = &StelApp::getInstance().getFontManager().getStandardFont(trans.getTrueLocaleName(), fontSize);
}


StelObjectP StarMgr::search(const QString& name) const
{
	// Use this QRegExp to extract the catalogue number and prefix
	QRegExp catRx("^(HP|HD|SAO)\\s*(\\d+)$");
	QString n = name.toUpper();
	n.replace('_', ' ');

	if (catRx.exactMatch(n))
	{
		QString cat = catRx.capturedTexts().at(1);
		if (cat=="HP")
			return searchHP(catRx.capturedTexts().at(2).toInt());
		else // currently we only support searching by string for HP catalogue
			return NULL;
	}
	else 
	{
		// Maybe the HP prefix is missing and we just have a number...
		bool ok;
		int num = n.toInt(&ok);
		if (!ok)
			return NULL;
		else
			return searchHP(num);
	}
}    

// Search the star by HP number
StelObjectP StarMgr::searchHP(int _HP) const {
  if (0 < _HP && _HP <= NR_OF_HIP) {
    const Star1 *const s = hip_index[_HP].s;
    if (s) {
      const SpecialZoneArray<Star1> *const a = hip_index[_HP].a;
      const SpecialZoneData<Star1> *const z = hip_index[_HP].z;
      return s->createStelObject(a,z);
    }
  }
  return StelObjectP();
}

StelObjectP StarMgr::searchByNameI18n(const QString& nameI18n) const
{
	QString objw = nameI18n.toUpper();

	// Search by HP number if it's an HP formated number
	QRegExp rx("^\\s*HP\\s*(\\d+)\\s*$", Qt::CaseInsensitive);
	if (rx.exactMatch(objw))
	{
		return searchHP(rx.capturedTexts().at(1).toInt());
	}

	// Search by I18n common name
	map<QString,int>::const_iterator it(common_names_index_i18n.find(objw));
	if (it!=common_names_index_i18n.end()) 
	{
		return searchHP(it->second);
	}

	// Search by sci name
	it = sci_names_index_i18n.find(objw);
	if (it!=sci_names_index_i18n.end()) 
	{
		return searchHP(it->second);
	}

	return StelObjectP();
}


StelObjectP StarMgr::searchByName(const QString& name) const
{
	QString objw = name.toUpper();

	// Search by HP number if it's an HP formated number
	QRegExp rx("^\\s*HP\\s*(\\d+)\\s*$", Qt::CaseInsensitive);
	if (rx.exactMatch(objw))
	{
		return searchHP(rx.capturedTexts().at(1).toInt());
	}


	/* Should we try this anyway?
	// Search by common name
	map<QString,int>::const_iterator it(common_names_index_i18n.find(objw));

	if (it!=common_names_index_i18n.end()) {
		return searchHP(it->second);
	} */

	// Search by sci name
	map<QString,int>::const_iterator it = sci_names_index_i18n.find(objw);
	if (it!=sci_names_index_i18n.end()) {
		return searchHP(it->second);
	}

	return StelObjectP();
}

//! Find and return the list of at most maxNbItem objects auto-completing
//! the passed object I18n name
QStringList StarMgr::listMatchingObjectsI18n(const QString& objPrefix, int maxNbItem) const 
{
	QStringList result;
	if (maxNbItem==0) return result;

	QString objw = objPrefix.toUpper();

	// Search for common names
	for (map<QString,int>::const_iterator it(common_names_index_i18n.lower_bound(objw));
	     it!=common_names_index_i18n.end();
	     it++) 
	{
		const QString constw(it->first.mid(0,objw.size()));
		if (constw==objw) {
			if (maxNbItem==0) break;
			result << getCommonName(it->second);
			maxNbItem--;
		} 
		else 
			break;
	}

	// Search for sci names
	for (map<QString,int>::const_iterator it(sci_names_index_i18n.lower_bound(objw));
	     it!=sci_names_index_i18n.end();
	     it++) 
	{
		const QString constw(it->first.mid(0,objw.size()));
		if (constw==objw) {
			if (maxNbItem==0) break;
			result << getSciName(it->second);
			maxNbItem--;
		} 
		else 
			break;
	}

	result.sort();
	return result;
}


//! Define font file name and size to use for star names display
void StarMgr::setFontSize(double newFontSize)
{
	fontSize = newFontSize;
	starFont = &StelApp::getInstance().getFontManager().getStandardFont(
		StelApp::getInstance().getLocaleMgr().getSkyLanguage(),fontSize);
}

void StarMgr::updateSkyCulture()
{
	QString skyCultureDir = StelApp::getInstance().getSkyCultureMgr().getSkyCultureDir();
	
	// Load culture star names in english
	try
	{
		load_common_names(StelApp::getInstance().getFileMgr().findFile("skycultures/" + skyCultureDir + "/star_names.fab"));
	}
	catch(exception& e)
	{
		qWarning() << "WARNING: could not load star_names.fab for sky culture " 
		           << skyCultureDir << ": " << e.what();	
	}
	
	try
	{
		load_sci_names(StelApp::getInstance().getFileMgr().findFile("stars/default/name.fab"));
	}
	catch(exception& e)
	{
		qWarning() << "WARNING: could not load scientific star names file: " << e.what();
	}

	// Turn on sci names/catalog names for western culture only
	setFlagSciNames(skyCultureDir.startsWith("western"));
	updateI18n();
}
