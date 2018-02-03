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

#include "SqlObjDataSource.h"
#include "Database/Database.h"
#include <inttypes.h>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
using boost::bad_lexical_cast;

namespace
{
	typedef boost::optional<Sqf::Value> PositionInfo;
	class PositionFixerVisitor : public boost::static_visitor<PositionInfo>
	{
	public:
		PositionInfo operator()(Sqf::Parameters& pos) const 
		{ 
			if (pos.size() != 3)
				return PositionInfo();

			try
			{
				double x = Sqf::GetDouble(pos[0]);
				double y = Sqf::GetDouble(pos[1]);
				double z = Sqf::GetDouble(pos[2]);

				if (x < 0 || y > 15360)
				{
					PositionInfo fixed(pos);
					pos.clear();
					return fixed;
				}
			}
			catch(const boost::bad_get&) {}

			return PositionInfo();
		}
		template<typename T> PositionInfo operator()(const T& other) const	{ return PositionInfo(); }
	};

	class WorldspaceFixerVisitor : public boost::static_visitor<PositionInfo>
	{
	public:
		PositionInfo operator()(Sqf::Parameters& ws) const 
		{ 
			if (ws.size() != 2)
				return PositionInfo();

			return boost::apply_visitor(PositionFixerVisitor(),ws[1]);
		}
		template<typename T> PositionInfo operator()(const T& other) const	{ return PositionInfo(); }
	};

	PositionInfo FixOOBWorldspace(Sqf::Value& v) { return boost::apply_visitor(WorldspaceFixerVisitor(),v); }
};

#include <Poco/Util/AbstractConfiguration.h>
/*
SqlGarageDataSource::SqlGarageDataSource(Poco::Logger& logger, shared_ptr<Database> db, const Poco::Util::AbstractConfiguration* conf) : SqlDataSource(logger, db)
{
	static const string defaultGarageTable = "garage";
	if (conf != NULL)
	{
		_garageTableName = getDB()->escape(conf->getString("Table", defaultGarageTable));
		_cleanupStoredDays = conf->getInt("CleanupVehStoredDays", 35);
	}
	else
	{
		_garageTableName = defaultGarageTable;
		_cleanupStoredDays = -1;
	}
	if (_cleanupStoredDays >= 0)
	{
		string CommonVGSQL = "FROM `" + _garageTableName + "` WHERE `DateStamp` < DATE_SUB(CURRENT_TIMESTAMP, INTERVAL " + lexical_cast<string>(_cleanupStoredDays) + " DAY)";

		int numToClean = 0;
		{
			auto numVehsToClean = getDB()->query(("SELECT COUNT(*) " + CommonVGSQL).c_str());
			if (numVehsToClean && numVehsToClean->fetchRow())
				numToClean = numVehsToClean->at(0).getInt32();
		}
		if (numToClean > 0)
		{
			_logger.information("Removing " + lexical_cast<string>(numToClean) + " placed objects older than " + lexical_cast<string>(_cleanupStoredDays) + " days");

			auto stmt = getDB()->makeStatement(_stmtVGCleanupStored, "DELETE " + CommonVGSQL);
			if (!stmt->directExecute())
				_logger.error("Error executing placed objects cleanup statement");
		}
	}

}
*/
SqlObjDataSource::SqlObjDataSource( Poco::Logger& logger, shared_ptr<Database> db, const Poco::Util::AbstractConfiguration* conf ) : SqlDataSource(logger,db)
{
	static const string defaultTable = "Object_DATA";
	static const string defaultGarageTable = "garage";
	if (conf != NULL)
	{
		_objTableName = getDB()->escape(conf->getString("Table",defaultTable));
		_cleanupPlacedDays = conf->getInt("CleanupPlacedAfterDays",6);
		_vehicleOOBReset = conf->getBool("ResetOOBVehicles",false);
		_maintenanceObjs = conf->getString("MaintenanceObjects","'Land_DZE_GarageWoodDoorLocked','Land_DZE_LargeWoodDoorLocked','Land_DZE_WoodDoorLocked','CinderWallDoorLocked_DZ','CinderWallDoorSmallLocked_DZ','Plastic_Pole_EP1_DZ'");
		_garageTableName = getDB()->escape(conf->getString("VGTable", defaultGarageTable));
		_cleanupStoredDays = conf->getInt("CleanupVehStoredDays", 35);
		_logObjCleanup = conf->getBool("LogObjectCleanup", false);
	}
	else
	{
		_objTableName = defaultTable;
		_cleanupPlacedDays = -1;
		_vehicleOOBReset = false;
		_maintenanceObjs = "'Land_DZE_GarageWoodDoorLocked','Land_DZE_LargeWoodDoorLocked','Land_DZE_WoodDoorLocked','CinderWallDoorLocked_DZ','CinderWallDoorSmallLocked_DZ','Plastic_Pole_EP1_DZ'";
		_garageTableName = defaultGarageTable;
		_cleanupStoredDays = -1;
		_logObjCleanup = false;
	}
}

void SqlObjDataSource::populateObjects( int serverId, ServerObjectsQueue& queue )
{

	//Prevent duplicate ObjectUID by setting ObjectUID = (ObjectID + 1)
	string commonUpdateUIDSql = " WHERE `Instance` = " + lexical_cast<string>(serverId) + " AND `ObjectID` <> 0 AND `ObjectID` <> (`ObjectUID` - 1)";
	int numIDUpdated = 0;
	{
		auto numObjsToUpdate = getDB()->query(("SELECT COUNT(*) FROM " + _objTableName + commonUpdateUIDSql).c_str());
		if (numObjsToUpdate && numObjsToUpdate->fetchRow())
			numIDUpdated = numObjsToUpdate->at(0).getInt32();
	}
	if (numIDUpdated > 0)
	{
		_logger.information("Updating " + lexical_cast<string>(numIDUpdated) + " Object_Data ObjectUID");
		auto stmt2 = getDB()->makeStatement(_stmtChangeObjectUID, "UPDATE " + _objTableName + " SET ObjectUID = (ObjectID + 1) " + commonUpdateUIDSql);
		if (!stmt2->directExecute())
			_logger.error("Error executing update ObjectUID statement");
	}

	if (_cleanupPlacedDays >= 0)
	{
		string commonSql = "FROM `" + _objTableName + "` WHERE `Instance` = " + lexical_cast<string>(serverId) +
			" AND `ObjectUID` <> 0 AND `CharacterID` <> 0" +
			" AND `Datestamp` < DATE_SUB(CURRENT_TIMESTAMP, INTERVAL " + lexical_cast<string>(_cleanupPlacedDays) + " DAY)" +
			" AND ( (`Inventory` IS NULL) OR (`Inventory` = '[]') " +
			" OR (`Classname` IN ( " + _maintenanceObjs + ") ))";

		int numCleaned = 0;
		{
			auto numObjsToClean = getDB()->query(("SELECT COUNT(*) "+commonSql).c_str());
			if (numObjsToClean && numObjsToClean->fetchRow())
				numCleaned = numObjsToClean->at(0).getInt32();
		}
		if (numCleaned > 0)
		{
			_logger.notice("Removing " + lexical_cast<string>(numCleaned) + " placed objects older than " + lexical_cast<string>(_cleanupPlacedDays) + " days");
			if (_logObjCleanup) {
				auto logObjsToClean = getDB()->query(("SELECT ObjectUID, Inventory, Classname, CharacterID, Worldspace, StorageCoins " + commonSql).c_str());
				while (logObjsToClean->fetchRow())
				{
					auto rowDel = logObjsToClean->fields();
					_logger.notice("OBJ CLEANUP DELETE. Classname: " + rowDel[2].getString() + " with inventory:" + lexical_cast<string>(lexical_cast<Sqf::Value>(rowDel[1].getString())) + " Object UID: " + lexical_cast<string>(rowDel[0].getInt64()) + " Character ID: " + lexical_cast<string>(rowDel[3].getInt64()) + " Storage Coins: " + lexical_cast<string>(rowDel[5].getInt64()) + " At Worldspace: " + lexical_cast<string>(lexical_cast<Sqf::Value>(rowDel[4].getString())));
				}
			}
			auto stmt = getDB()->makeStatement(_stmtDeleteOldObject, "DELETE "+commonSql);
			if (!stmt->directExecute())
				_logger.error("Error executing placed objects cleanup statement");
		}
	}

	//VG DATA CLEAN
	if (_cleanupStoredDays >= 0)
	{
		string CommonVGSQL = "FROM `" + _garageTableName + "` WHERE `DateMaintained` < DATE_SUB(CURRENT_TIMESTAMP, INTERVAL " + lexical_cast<string>(_cleanupStoredDays) + " DAY)";

		int numToClean = 0;
		{
			auto numVehsToClean = getDB()->query(("SELECT COUNT(*) " + CommonVGSQL).c_str());
			if (numVehsToClean && numVehsToClean->fetchRow())
				numToClean = numVehsToClean->at(0).getInt32();
		}
		if (numToClean > 0)
		{
			_logger.notice("Removing " + lexical_cast<string>(numToClean) + " virtual garage vehicles stored for " + lexical_cast<string>(_cleanupStoredDays) + " days");
			if (_logObjCleanup) {
				auto logVGObjsToClean = getDB()->query(("SELECT ObjUID, Inventory, Classname, CharacterID, PlayerUID, Name " + CommonVGSQL).c_str());
				while (logVGObjsToClean->fetchRow())
				{
					auto rowVGDel = logVGObjsToClean->fields();
					_logger.notice("VG CLEANUP DELETE. Classname: " + rowVGDel[2].getString() + " with inventory:" + lexical_cast<string>(lexical_cast<Sqf::Value>(rowVGDel[1].getString())) + " Object UID: " + rowVGDel[0].getString() + " Character ID: " + lexical_cast<string>(rowVGDel[3].getInt64()) + " Storing player name: " + rowVGDel[5].getString() + " Storing player UID: " + rowVGDel[4].getString());
				}
			}
			auto stmt = getDB()->makeStatement(_stmtVGCleanupStored, "DELETE " + CommonVGSQL);
			if (!stmt->directExecute())
				_logger.error("Error executing placed objects cleanup statement");
		}
	}
	//END VG
	
	auto worldObjsRes = getDB()->queryParams("SELECT `ObjectID`, `Classname`, `CharacterID`, `Worldspace`, `Inventory`, `Hitpoints`, `Fuel`, `Damage`, `StorageCoins` FROM `%s` WHERE `Instance`=%d AND `Classname` IS NOT NULL AND `Damage` < 1", _objTableName.c_str(), serverId);
	if (!worldObjsRes)
	{
		_logger.error("Failed to fetch objects from database");
		return;
	}
	while (worldObjsRes->fetchRow())
	{
		auto row = worldObjsRes->fields();

		Sqf::Parameters objParams;
		objParams.push_back(string("OBJ"));

		int objectId = row[0].getInt32();
		objParams.push_back(lexical_cast<string>(objectId)); //objectId should be stringified
		try
		{
			objParams.push_back(row[1].getString()); //classname
			objParams.push_back(lexical_cast<string>(row[2].getInt64())); //ownerId should be stringified

			Sqf::Value worldSpace = lexical_cast<Sqf::Value>(row[3].getString());
			if (_vehicleOOBReset && row[2].getInt64() == 0) // no owner = vehicle
			{
				PositionInfo posInfo = FixOOBWorldspace(worldSpace);
				if (posInfo.is_initialized())
					_logger.information("Reset ObjectID " + lexical_cast<string>(objectId) + " (" + row[1].getString() + ") from position " + lexical_cast<string>(*posInfo));

			}
			objParams.push_back(worldSpace);

			//Inventory can be NULL
			{
				string invStr = "[]";
				if (!row[4].isNull())
					invStr = row[4].getString();

				objParams.push_back(lexical_cast<Sqf::Value>(invStr));
			}	
			objParams.push_back(lexical_cast<Sqf::Value>(row[5].getCStr()));
			objParams.push_back(row[6].getDouble());
			objParams.push_back(row[7].getDouble());
			objParams.push_back(row[8].getInt64()); //Coins
		}
		catch (const bad_lexical_cast&)
		{
			_logger.error("Skipping ObjectID " + lexical_cast<string>(objectId) + " load because of invalid data in db");
			continue;
		}

		queue.push(objParams);
	}
}
void SqlObjDataSource::populateTraderObjects( Int64 characterId, ServerObjectsQueue& queue )
{	
	
	auto worldObjsRes = getDB()->queryParams("SELECT `id`, `item`, `qty`, `buy`, `sell`, `order`, `tid`, `afile` FROM `Traders_DATA` WHERE `tid`=%d", characterId);

	while (worldObjsRes->fetchRow())
	{
		auto row = worldObjsRes->fields();

		Sqf::Parameters objParams;
		objParams.push_back(row[0].getInt32());
		
		// `item` varchar(255) NOT NULL COMMENT '[Class Name,1 = CfgMagazines | 2 = Vehicle | 3 = Weapon]',
		// `qty` int(8) NOT NULL COMMENT 'amount in stock available to buy',
		// `buy`  varchar(255) NOT NULL COMMENT '[[Qty,Class,Type],]',
		// `sell`  varchar(255) NOT NULL COMMENT '[[Qty,Class,Type],]',
		// `order` int(2) NOT NULL DEFAULT '0' COMMENT '# sort order for addAction menu',
		// `tid` int(8) NOT NULL COMMENT 'Trader Menu ID',
		// `afile` varchar(64) NOT NULL DEFAULT 'trade_items',

		//get stuff from row
		objParams.push_back(lexical_cast<Sqf::Value>(row[1].getString()));  // item
		objParams.push_back(row[2].getInt32()); // qty
		objParams.push_back(lexical_cast<Sqf::Value>(row[3].getString()));  // buy
		objParams.push_back(lexical_cast<Sqf::Value>(row[4].getString()));  // sell
		objParams.push_back(row[5].getInt32()); // order
		objParams.push_back(row[6].getInt32()); // tid
		objParams.push_back(row[7].getString()); // afile

		queue.push(objParams);
	}
}

bool SqlObjDataSource::updateObjectInventory(int serverId, Int64 objectIdent, bool byUID, const Sqf::Value& inventory)
{
	unique_ptr<SqlStatement> stmt;
	if (byUID)
		stmt = getDB()->makeStatement(_stmtUpdateObjectbyUID, "UPDATE `" + _objTableName + "` SET `Inventory` = ? WHERE `ObjectUID` = ? AND `Instance` = ?");
	else
		stmt = getDB()->makeStatement(_stmtUpdateObjectByID, "UPDATE `" + _objTableName + "` SET `Inventory` = ? WHERE `ObjectID` = ? AND `Instance` = ?");

	stmt->addString(lexical_cast<string>(inventory));
	stmt->addInt64(objectIdent);
	stmt->addInt32(serverId);

	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::updateObjectInventoryWCoins(int serverId, Int64 objectIdent, bool byUID, const Sqf::Value& inventory, Int64 coinsValue)
{
	unique_ptr<SqlStatement> stmt;
	if (byUID)
		stmt = getDB()->makeStatement(_stmtUpdateObjectbyUIDCoins, "UPDATE `" + _objTableName + "` SET `Inventory` = ?, `StorageCoins` = ? WHERE `ObjectUID` = ? AND `Instance` = ?");
	else
		stmt = getDB()->makeStatement(_stmtUpdateObjectByIDCoins, "UPDATE `" + _objTableName + "` SET `Inventory` = ?, `StorageCoins` = ? WHERE `ObjectID` = ? AND `Instance` = ?");

	stmt->addString(lexical_cast<string>(inventory));
	stmt->addInt64(coinsValue);
	stmt->addInt64(objectIdent);
	stmt->addInt32(serverId);

	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::deleteObject( int serverId, Int64 objectIdent, bool byUID )
{
	unique_ptr<SqlStatement> stmt;
	if (byUID)
		stmt = getDB()->makeStatement(_stmtDeleteObjectByUID, "DELETE FROM `"+_objTableName+"` WHERE `ObjectUID` = ? AND `Instance` = ?");
	else
		stmt = getDB()->makeStatement(_stmtDeleteObjectByID, "DELETE FROM `"+_objTableName+"` WHERE `ObjectID` = ? AND `Instance` = ?");

	stmt->addInt64(objectIdent);
	stmt->addInt32(serverId);

	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::updateDatestampObject( int serverId, Int64 objectIdent, bool byUID )
{
	unique_ptr<SqlStatement> stmt;
	if (byUID)
		stmt = getDB()->makeStatement(_stmtUpdateDatestampObjectByUID, "UPDATE `" + _objTableName + "` SET `Datestamp` = CURRENT_TIMESTAMP, `Damage` = '0' WHERE `ObjectUID` = ? AND `Instance` = ?");
	else
		stmt = getDB()->makeStatement(_stmtUpdateDatestampObjectByID, "UPDATE `" + _objTableName + "` SET `Datestamp` = CURRENT_TIMESTAMP, `Damage` = '0' WHERE `ObjectID` = ? AND `Instance` = ?");

	stmt->addInt64(objectIdent);
	stmt->addInt32(serverId);

	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::updateVehicleMovement( int serverId, Int64 objectIdent, const Sqf::Value& worldspace, double fuel )
{
	auto stmt = getDB()->makeStatement(_stmtUpdateVehicleMovement, "UPDATE `"+_objTableName+"` SET `Worldspace` = ? , `Fuel` = ? WHERE `ObjectID` = ?  AND `Instance` = ?");
	stmt->addString(lexical_cast<string>(worldspace));
	stmt->addDouble(fuel);
	stmt->addInt64(objectIdent);
	stmt->addInt32(serverId);
	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::updateVehicleStatus( int serverId, Int64 objectIdent, const Sqf::Value& hitPoints, double damage )
{
	auto stmt = getDB()->makeStatement(_stmtUpdateVehicleStatus, "UPDATE `"+_objTableName+"` SET `Hitpoints` = ? , `Damage` = ? WHERE `ObjectID` = ? AND `Instance` = ?");
	stmt->addString(lexical_cast<string>(hitPoints));
	stmt->addDouble(damage);
	stmt->addInt64(objectIdent);
	stmt->addInt32(serverId);
	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}

bool SqlObjDataSource::createObject( int serverId, const string& className, double damage, Int64 characterId, 
	const Sqf::Value& worldSpace, const Sqf::Value& inventory, const Sqf::Value& hitPoints, double fuel, Int64 uniqueId )
{
	auto stmt = getDB()->makeStatement(_stmtCreateObject, 
		"INSERT INTO `"+_objTableName+"` (`ObjectUID`, `Instance`, `Classname`, `Damage`, `CharacterID`, `Worldspace`, `Inventory`, `Hitpoints`, `Fuel`, `Datestamp`) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)");

	stmt->addInt64(uniqueId);
	stmt->addInt32(serverId);
	stmt->addString(className);
	stmt->addDouble(damage);
	stmt->addInt64(characterId);
	stmt->addString(lexical_cast<string>(worldSpace));
	stmt->addString(lexical_cast<string>(inventory));
	stmt->addString(lexical_cast<string>(hitPoints));
	stmt->addDouble(fuel);
	bool exRes = stmt->execute();
	poco_assert(exRes == true);

	return exRes;
}

Sqf::Value SqlObjDataSource::fetchObjectId( int serverId, Int64 objectIdent )
{
	Sqf::Parameters retVal;
	//get details from db
	//auto worldObjsRes = getDB()->queryParams("SELECT `ObjectID` FROM `%s` WHERE `Instance` = %d AND `ObjectUID` = %" PRIu64 "", _objTableName.c_str(), serverId, objectIdent);

	auto worldObjsRes = getDB()->queryParams("CALL retObjID('%s', %d, %" PRIu64 ", @OID)", _objTableName.c_str(), serverId, objectIdent);

	if (worldObjsRes && worldObjsRes->fetchRow())
	{
		int objectid = 0;
		//get stuff from row
		objectid = worldObjsRes->at(0).getInt32();

		if (objectid != 0)
		{
			retVal.push_back(string("PASS"));
			retVal.push_back(lexical_cast<string>(objectid));
		}
		else
		{
			retVal.push_back(string("ERROR"));
		}
	}
	else
	{
		retVal.push_back(string("ERROR"));
	}

	return retVal;
}

//Virtual Garage Stuff

bool SqlObjDataSource::UpdateVGStoreVeh(const string& PlayerUID, const string& PlayerName, const string& DisplayName, const string& ClassName, const string& DateStored, const string& ObjCID, const Sqf::Value& inventory, const Sqf::Value& hitPoints, double fuel, double Damage, const string& Colour, const string& Colour2, const string& VGServerKey, const string& ObjUID, const Sqf::Value& inventoryCount)
{
	//INSERT INTO garage (PlayerUID, Name, DisplayName, Classname, DateStored, CharacterID, Inventory, Hitpoints, Fuel, Damage, Colour, Colour2) VALUES ('%1','%2','%3','%4','%5','%6','%7','%8','%9','%10','%11','%12')
	//select count(*) FROM `" + _garageTableName + "` WHERE `serverKey` = " + VGServerKey + " `ObjUID` = " + ObjUID)
	Sqf::Parameters myRetVal;
	bool exRes = false;
	auto VGDupliateCheck = getDB()->query((_stmtVGDupliateCheck, "select count(*) FROM `" + _garageTableName + "` WHERE `serverKey` = '" + VGServerKey + "' AND `ObjUID` = '" + ObjUID + "'").c_str());

	if (VGDupliateCheck && VGDupliateCheck->fetchRow())
	{
		if (VGDupliateCheck->at(0).getInt32() < 1) {
			unique_ptr<SqlStatement> vgupdatestmt;
			vgupdatestmt = getDB()->makeStatement(_stmtVGStoreVeh,
				"INSERT INTO `" + _garageTableName + "` (`PlayerUID`, `Name`, `DisplayName`, `Classname`, `Datestamp`, `DateStored`, `DateMaintained`, `CharacterID`, StorageCounts, `Inventory`, `Hitpoints`, `Fuel`, `Damage`, `Colour`, `Colour2`, `serverKey`, `ObjUID`) "
				"VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP, ?, CURRENT_TIMESTAMP, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

			vgupdatestmt->addString(PlayerUID);
			vgupdatestmt->addString(PlayerName);
			vgupdatestmt->addString(DisplayName);
			vgupdatestmt->addString(ClassName);
			vgupdatestmt->addString(DateStored);
			vgupdatestmt->addString(ObjCID);
			vgupdatestmt->addString(lexical_cast<string>(inventoryCount));
			vgupdatestmt->addString(lexical_cast<string>(inventory));
			vgupdatestmt->addString(lexical_cast<string>(hitPoints));
			vgupdatestmt->addDouble(fuel);
			vgupdatestmt->addDouble(Damage);
			vgupdatestmt->addString(Colour);
			vgupdatestmt->addString(Colour2);
			vgupdatestmt->addString(VGServerKey);
			vgupdatestmt->addString(ObjUID);
			exRes = vgupdatestmt->execute();
			poco_assert(exRes == true);
		}
		else {
			_logger.error("Duplicate object NOT stored in virtual garage. storage attempt by player with UID: " + PlayerUID + " ObjectID: " + ObjUID + " VG_ServerKey: " + VGServerKey);
		}
	}
	return exRes;
}

bool SqlObjDataSource::DeleteMyVGVeh(Int64 VehID)
{
	//"DELETE FROM garage WHERE ID='%1'", _id

	unique_ptr<SqlStatement> vgdelstmt;
	vgdelstmt = getDB()->makeStatement(_stmtVGDelVeh, "DELETE FROM `" + _garageTableName + "` WHERE `ID` = ?");

	vgdelstmt->addInt64(VehID);

	bool RetSTMT = vgdelstmt->execute();
	poco_assert(RetSTMT == true);

	return RetSTMT;
}

Sqf::Value SqlObjDataSource::GetMyVGVehs(const string& playerUID, const string& sortColumn)
{
	//["SELECT id, classname, Inventory, CharacterID,DateStored FROM `%s` WHERE PlayerUID='%s' ORDER BY DisplayName",_playerUID];
	Sqf::Parameters retVGGetVal;

	auto VGRetMyVeh = getDB()->queryParams(("SELECT id, classname, StorageCounts, CharacterID, DateStored, DateMaintained FROM `" + _garageTableName + "` WHERE PlayerUID = '" + playerUID + "' ORDER BY `" + sortColumn + "`").c_str());
	while (VGRetMyVeh->fetchRow())
	{
		Sqf::Parameters retVGGetValTemp;
		auto row = VGRetMyVeh->fields();

		retVGGetValTemp.push_back(row[0].getInt32());							 //int(11) ID
		retVGGetValTemp.push_back(row[1].getString());							 //varchar classname
		retVGGetValTemp.push_back(lexical_cast<Sqf::Value>(row[2].getString())); //varchar StorageCounts
		retVGGetValTemp.push_back(row[3].getInt32());							 //bigint(20) unsigned CharacterID
		retVGGetValTemp.push_back(row[4].getString());							 //varchar DateStored
		retVGGetValTemp.push_back(row[5].getString());							 //timestamp DateMaintained
		retVGGetValTemp.push_back(_cleanupStoredDays);

		retVGGetVal.push_back(retVGGetValTemp);
	}
	return retVGGetVal;
}

Sqf::Parameters SqlObjDataSource::VgSelectSpawnVeh(const Sqf::Value& worldSpace, Int64 VehID, Int64 uniqueId)
{
	//"SELECT classname, CharacterID, Inventory, Hitpoints, Fuel, Damage, Colour, Colour2, serverKey FROM garage WHERE ID='%1'"
	Sqf::Parameters myRetVal;
	auto VGObjID = getDB()->queryParams("SELECT classname, CharacterID, Inventory, Hitpoints, Fuel, Damage, Colour, Colour2, serverKey, ObjUID FROM `%s` WHERE ID='%d'", _garageTableName.c_str(), VehID);

	if (VGObjID && VGObjID->fetchRow())
	{
		//get stuff from row
		string classname = VGObjID->at(0).getString();
		Int64 CharacterID = VGObjID->at(1).getInt64();
		Sqf::Value Inventory = lexical_cast<Sqf::Value>(VGObjID->at(2).getString());
		Sqf::Value hitPoints = lexical_cast<Sqf::Value>(VGObjID->at(3).getString());
		double fuel = VGObjID->at(4).getDouble();
		double damage = VGObjID->at(5).getDouble();
		string colour = VGObjID->at(6).getString();
		string colour2 = VGObjID->at(7).getString();
		string VGServerKey = VGObjID->at(8).getString();
		string VG_ObjectID = VGObjID->at(9).getString();

		myRetVal.push_back(string("PASS"));
		myRetVal.push_back(classname);
		myRetVal.push_back(CharacterID);
		myRetVal.push_back(Inventory);
		myRetVal.push_back(hitPoints);
		myRetVal.push_back(fuel);
		myRetVal.push_back(damage);
		myRetVal.push_back(colour);
		myRetVal.push_back(colour2);
		myRetVal.push_back(VGServerKey);
		myRetVal.push_back(VG_ObjectID);
	}
	else
	{
		myRetVal.push_back(string("ERROR"));
	}
	return myRetVal;
}

bool SqlObjDataSource::MaintainMyVGVeh(const string& PlayerUID)
{
	unique_ptr<SqlStatement> vgmaintainstmt;
	vgmaintainstmt = getDB()->makeStatement(_stmtVGMaintainVeh, "UPDATE `" + _garageTableName + "` SET `DateMaintained` = CURRENT_TIMESTAMP WHERE `PlayerUID` = ?");

	vgmaintainstmt->addString(PlayerUID);

	bool STMTExec = vgmaintainstmt->execute();
	poco_assert(STMTExec == true);

	return STMTExec;
}