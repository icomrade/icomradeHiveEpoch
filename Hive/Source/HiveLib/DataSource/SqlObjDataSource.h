/*
* Copyright (C) 2009-2012 Rajko Stojadinovic <http://github.com/rajkosto/hive>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#pragma once

#include "SqlDataSource.h"
#include "ObjDataSource.h"
#include "Database/SqlStatement.h"

namespace Poco { namespace Util { class AbstractConfiguration; }; };
class SqlObjDataSource : public SqlDataSource, public ObjDataSource
{
public:
	SqlObjDataSource(Poco::Logger& logger, shared_ptr<Database> db, const Poco::Util::AbstractConfiguration* conf);
	~SqlObjDataSource() {}

	void populateObjects( int serverId, ServerObjectsQueue& queue ) override;

	void populateTraderObjects( Int64 characterId, ServerObjectsQueue& queue ) override;

	bool updateObjectInventory( int serverId, Int64 objectIdent, bool byUID, const Sqf::Value& inventory ) override;
	bool updateObjectInventoryWCoins(int serverId, Int64 objectIdent, bool byUID, const Sqf::Value& inventory, Int64 coinsValue ) override;
	bool deleteObject( int serverId, Int64 objectIdent, bool byUID ) override;
	bool updateDatestampObject( int serverId, Int64 objectIdent, bool byUID ) override;
	bool updateVehicleMovement( int serverId, Int64 objectIdent, const Sqf::Value& worldspace, double fuel ) override;
	bool updateVehicleStatus( int serverId, Int64 objectIdent, const Sqf::Value& hitPoints, double damage ) override;
	bool createObject( int serverId, const string& className, double damage, Int64 characterId, 
		const Sqf::Value& worldSpace, const Sqf::Value& inventory, const Sqf::Value& hitPoints, double fuel, Int64 uniqueId ) override;
	Sqf::Value fetchObjectId( int serverId, Int64 objectIdent ) override;
	//VG
	bool UpdateVGStoreVeh(const string& PlayerUID, const string& PlayerName, const string& DisplayName, const string& ClassName, const string& DateStored, const string& ObjCID, const Sqf::Value& inventory, 
		const Sqf::Value& hitPoints, double fuel, double Damage, const string& Colour, const string& Colour2, const string& VGServerKey, const string& ObjUID, const Sqf::Value& inventoryCount) override;

	Sqf::Parameters VgSelectSpawnVeh(const Sqf::Value& worldSpace, Int64 VehID, Int64 uniqueId) override;
	bool DeleteMyVGVeh(Int64 VehID) override;
	Sqf::Value GetMyVGVehs(const string& playerUID, const string& sortColumn) override;
	bool MaintainMyVGVeh(const string& PlayerUID) override;
private:
	string _objTableName;
	int _cleanupPlacedDays;
	bool _vehicleOOBReset;
	string _maintenanceObjs;
	string _garageTableName;
	int _cleanupStoredDays;
	bool _logObjCleanup;

	//statement ids
	SqlStatementID _stmtChangeObjectUID;
	SqlStatementID _stmtDeleteOldObject;
	SqlStatementID _stmtUpdateObjectbyUID;
	SqlStatementID _stmtUpdateObjectByID;
	SqlStatementID _stmtUpdateObjectByIDCoins;
	SqlStatementID _stmtUpdateObjectbyUIDCoins;
	SqlStatementID _stmtDeleteObjectByUID;
	SqlStatementID _stmtDeleteObjectByID;
	SqlStatementID _stmtUpdateDatestampObjectByUID;
	SqlStatementID _stmtUpdateDatestampObjectByID;
	SqlStatementID _stmtUpdateVehicleMovement;
	SqlStatementID _stmtUpdateVehicleStatus;
	SqlStatementID _stmtCreateObject;
	SqlStatementID _stmtVGStoreVeh;
	SqlStatementID _stmtVGDelVeh;
	SqlStatementID _stmtVGCleanupStored;
	SqlStatementID _stmtVGDupliateCheck;
	SqlStatementID _stmtVGMaintainVeh;
}; 
/*
class SqlGarageDataSource : public SqlDataSource, public GarageDataSource
{
public:
	SqlGarageDataSource(Poco::Logger& logger, shared_ptr<Database> db, const Poco::Util::AbstractConfiguration* conf);
	~SqlGarageDataSource() {}

	bool UpdateVGStoreVeh(const string& PlayerUID, const string& PlayerName, const string& DisplayName, const string& ClassName, const string& DateStored,
		const string& ObjCID, const Sqf::Value& inventory, const Sqf::Value& hitPoints, double fuel, double Damage, const string& Colour, const string& Colour2, const string& VGServerKey, const string& ObjUID) override;

	Sqf::Parameters VgSelectSpawnVeh(const Sqf::Value& worldSpace, const string& VehID, Int64 uniqueId) override;
	bool DeleteMyVGVeh(const string& VehID) override;
	Sqf::Value GetMyVGVehs(const string& playerUID) override;

private:
	string _garageTableName;
	int _cleanupStoredDays;

	SqlStatementID _stmtVGStoreVeh;
	SqlStatementID _stmtVGDelVeh;
	SqlStatementID _stmtVGCleanupStored;
};
*/