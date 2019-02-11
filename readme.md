# Dependencies
* The instructions for buiulding Dependencies are based on VS 2017, modify MSVC versions according to your VS version
* Unless otherwise noted this guide is performed from the VS tools command prompt
* This project reuires visual studio to be run as an administrator!
1. Copy the HiveDeps folder into your C:\ drive

2. Install Python (any version works, use latest 3.x), and check add to PATH. click disable PATH limit then close installer https://www.python.org/downloads/

3. Download and build boost http://www.boost.org/ in the HiveDeps folder (We need the libs, prebuilt packages are not acceptable)
	* download the latest version of boost
	* Extract the files (not the root folder) to your C:\HiveDeps\boost directory

  #### Building Boost
  1. open up your visual studio command prompt
  2. ```CD C:\HiveDeps\boost```
  3. ```bootstrap.bat```
  4. ```b2.exe toolset=msvc-14.1 --build-type=complete variant=release,debug runtime-link=shared,static link=static threading=multi address-model=32 --without-log --stagedir=lib\x86\v141 --build-dir=out\x86\v141```
	  * NOTE: if the boost build produces libs with VC140 you will need to rename them to your compiler version for VC140 -> VC141 run the following in the command prompt (CD to the lib dir) https://pastebin.com/Gp5gPTLa

#### Building POCO
1. Download Poco here (We use 1.7.8): https://github.com/pocoproject/poco/releases and extract files to C:\HiveDeps\poco
2. POCO requires OpenSSL, MySQL Connector and ODBC (part of Windows SDK)
3. install Windows 8.1 SDK from this link if building fails https://developer.microsoft.com/en-us/windows/downloads/windows-8-1-sdk
4. Poco requires OpenSSL, but thankfully the Poco team has made a repository to easily build it on our own.
    * download this repository as a zip https://github.com/pocoproject/openssl/tree/master
    * extract the contents of the zip to C:\HiveDeps\poco\openssl (the files will be zipped inside a folder 'openssl-master' open that folder then extract the files from inside it to the poco\openssl directory)
    * navigate in your VS build CMD and build:
    * ``` CD C:\HiveDeps\poco\openssl ```
    * ```buildall.cmd```
5. Download and build the MySQL client connector in the C:\HiveDeps\MySQL folder
[REQUIRES CMAKE]
    * https://downloads.mysql.com/archives/c-c/ Select operating system: 'source code' and download the windows version
    * Edit the Poco buildwin.cmd file to set MYSQL_DIR=C:\HiveDeps\MySQL
    * To build:
    * (if your CMake path is not set) @set path=C:\Program Files (x86)\CMake\bin;%path%
    * ```CD C:\HiveDeps\MySQL```
    * ```cmake```
    * Open ALL_BUILD.vcxproj and build the RELEASE VERSION of the solution
    * Copy the files in  C:\HiveDeps\MySQL\libmysql\Release to your C:\HiveDeps\poco\lib folder
    * in the C:\HiveDeps\MySQL folder create a new lib folder
    * Copy the files in the following directors to C:\HiveDeps\MySQL\
    1. C:\HiveDeps\MySQL\libmysql\Release + C:\HiveDeps\MySQL\libmysql\Debug
    * download this repo, https://github.com/rajkosto/deps-mysql and merge the include directories. DO NOT OVERWRITE ANY FILES
    * NOTE you need to clean and rebuild a Dynamically linked version of MySQL for the hive AFTER building poco! This is not covered, since it's actually not necessary unless you change the DatabaseMySQL.dll code
    2. ```CD C:\HiveDeps\poco```
    3. ```buildwin 150 build all both Win32 samples```
    * Note: Replace 150 with your VS MSVC version

#### Building intel TBB
1. download from https://www.threadingbuildingblocks.org/download#stable-releases (guide is using 4.4 u5)
2. Extract to C:\HiveDeps\TBB
3. open C:\HiveDeps\TBB\build\vs2012\makefile.sln and update the solutions versions if prompted
4. Save all and close visual studio
5. CD C:\HiveDeps\TBB
6. gmake
7. Your output will be: C:\HiveDeps\TBB\build\windows_ia32_cl_vc14_release

#### Building detours
1. download the latest version of Microsoft Detours https://www.microsoft.com/en-us/download/details.aspx?id=52586 (extract to C:\kfw-2.6.5),
2. Extract files to the C:\HiveDeps\Detours folder
3. CD C:\HiveDeps\Detours\src
4. Overwrite your Makefile with the one in the root of the Detours folder in this Github (LINK)
4. Unset the Read Only property on MAKEFILE and edit the CFLAGS line to read ```CFLAGS=/W4 /WX /Zi /MTd /Gy /Gm- /Zl /Od /DDETOURS_BITS=$(DETOURS_BITS)```
5. ```nmake```

#### Building PostgreSQL
** PostgreSQL compilation is not covered in this tutorial as many users do not use it. You may use Rajkosto's Poco-Deps repository on Github**
** IN VS Right click on the PostgreSQL project and unload it **

#### Finally building the Hive
1. resolve your lib and include paths
    * if you are using the windows tooal and apps SDK version 10 you will need to add an include path ```$(UniversalCRT_IncludePath)``` to all projects
    * right click each project > properties > C/C++ > General > add ```$(UniversalCRT_IncludePath)``` to your additional includes directory (replace [VER] with your SDK version path)
    * replace text in the additional includes field with: ```%(AdditionalIncludeDirectories)$(UniversalCRT_IncludePath)C:\HiveDeps\boost;C:\Program Files (x86)\Windows Kits\10\Include\[VER]\ucrt```
    1. you need to edit the boost lib directory to the Library Directories field of project properties -> VC++ Directories. where VER is your MSVC VER
        * AND Add the ucrt libs directory to the field if you are using 10 SDK
        * add first to the input field, keep any text already in the box ```C:\HiveDeps\boost\lib\x86\VER\lib;```
        * FOR SDK 10 USERS ONLY! ```C:\HiveDeps\boost\lib\x86\VER\lib;C:\Program Files (x86)\Windows Kits\10\Lib\VER\ucrt\x86;```
    2. Edit TBB.props file if it does not reflect you current TBB lib path
        * ```$(MSBuildThisFileDirectory)build\windows_ia32_cl_vc14_release;``` ----> ```$(MSBuildThisFileDirectory)build\windows_ia32_cl_[YOUR VERSION]_release;```
2. if you recieve any missing include errors, google them, chances are the VS installation requires additional components

# Common Errors
1. Double check your include and library paths if you are missing any .lib .h files, or links. You need to change paths in this guide to YOUR actual path and version after building the Dependencies
	* Also make sure your .props files are in the correct places and are up to date
2. ```:VCEnd exited with error code #```
	* Run VS as administrator, make sure you have python! Clean solution and rebuild
	* Ensure git is part of your environment variables (this project requires the git command for versioning) see the instructions here for directions: http://stackoverflow.com/questions/26620312/installing-git-in-path-with-github-client-for-windows
	* Environment variable edits require application restart!
	* You must be working out of a cloned repository using the github app for windows
	* The file ./source/HiveLib/version_gen.sh and version_gen.py may need to be edited ```gitVer=`git rev-list HEAD | head -n 1``` ---> ```gitVer=`git rev-list HEAD ^origin | head -n 1```
3. If you are building a debug build, you will need to change all of the static paths to the debug libraries, since I linked to the release libs in the props, and guide above
4. If your build is failing despite following the directions, ensure you select the release|Win32 Active config setting
