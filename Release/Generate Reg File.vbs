Set WshShell = WScript.CreateObject("WScript.Shell")
Set WshFSO = WScript.CreateObject("Scripting.FileSystemObject")
Set RegFile = WshFSO.CreateTextFile("DLL2IMPLIB.reg", True, True)
StrCD = WshShell.CurrentDirectory
StrCD = Replace(StrCD, "\", "\\")
' WScript.Echo (StrCD)

RegFile.WriteLine("REGEDIT4")
RegFile.WriteLine("[HKEY_CLASSES_ROOT\dllfile\shell\DLL2IMPLIB]")
RegFile.WriteLine("@=""DLL2IMPLIB""")
RegFile.WriteLine("""Icon""=""" + StrCD + "\\Arrow_Right.ico""")
RegFile.WriteLine("[HKEY_CLASSES_ROOT\dllfile\shell\DLL2IMPLIB\Command]")

RegFile.WriteLine("@=""" + StrCD + "\\DLL2IMPLIB.exe %V""")
RegFile.Close