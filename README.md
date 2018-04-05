Disclaimer
===========
***Use at your own risk.***

As a proof of concept, this project installs a Windows driver that might possibly damage your system (particularly the filesystem). You are advised to deploy this in to a test machine (such as a VM). Also, at the time of writing I have not really looked into how to uninstall this thing--we're just trying to get a thing working.  
We do not make any warranties about the reliability of this driver. **Any action you take upon this source code is strictly at your own risk** and we will not be liable for any losses and damages in connection with the use of this project.

Description
============
[A Windows filesystem minifilter driver](https://msdn.microsoft.com/library/windows/hardware/ff540402) [[alt-link]](https://docs.microsoft.com/en-us/windows-hardware/drivers/ifs/file-system-minifilter-drivers).  
Currently does nothing but write kernel debug output when standard driver routines are ran.  
Sometime in the future, should detect and hide marked text from the user.

Background
===========
We were reverse engineering a driver until we realised something: RE might go smoother if we knew what a plain driver looked like.

Well the semester is about to end, but maybe someone in the future can continue to look into kernel debugging and reverse engineering for Windows!

Setup
======
Note: Instructions below refers to 'debug host' as the machine building and/or running the debugger;  
'debug target' refers to the machine with the driver installed and being debugged.

This driver targets Windows 10, Version 1703.  
Supposedly this means that this driver should work for future versions of Windows. Hopefully.

Why was this chosen? Because driver development for win10-1709 using VS2017 is not working right now (can personally confirm).

Prerequisites
--------------
Prerequisites setup below is basically copied from here:  
https://docs.microsoft.com/en-us/windows-hardware/drivers/other-wdk-downloads  
(just in case of link rot) <!-- like many MSDN links -->

- Visual Studio 2015
	- Available at  
		https://www.visualstudio.com/vs/older-downloads/  
		_Requires a Microsoft account._
- Windows 10, Version 1703 (10.0.15063) SDK
	- Available at  
		https://developer.microsoft.com/en-us/windows/downloads/sdk-archive
	- Direct links: 
		[[EXE]](https://go.microsoft.com/fwlink/p/?LinkId=845298) 
		[[ISO]](https://go.microsoft.com/fwlink/p/?LinkId=845299)
- Windows 10.0.15063 WDK (Windows Driver Kit)
	- .EXE web downloader available at  
		https://go.microsoft.com/fwlink/p/?LinkID=845980

Deployment Instructions
------------------------
### Manual
It is generally recommended to use remote kernel debugging, but some situations may prevent you from doing so.

To observe kernel debug output from this driver, you may wish to enable kernel debugging on the debug target to connect to it.

1. Build Visual Studio project.  
	This produces a few files to .zip up and transfer to the debug target:  
	![A .cer, .inf, .pdb, and .sys file is produced](docs-assets/build_output.png)
1. On the debug target, we will disable signed driver enforcement. This a security feature since Windows Vista(ish?) to help defend against rootkits. Much like what we're installing right now.  
	Not disabling this will have Windows blocking either installation of the driver or its startup.  
	_As an administrator,_ run the following commands:
	```
	bcdedit.exe /set nointegritychecks off
	bcdedit.exe /set testsigning on
	```
1. Reboot the debug target. Windows should permit the installation of any driver for this session.
1. Navigate to the location on the debug target where you copied the build output files.  
	Right-click the setup information file 'FsFltStrhide.inf' and select 'Install'.
1. Enjoy while hot.
