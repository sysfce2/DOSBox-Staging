//css_ref System.Core.dll;
//css_ref System.Windows.Forms.dll;
//css_ref System.Drawing.dll;
//css_ref Wix_bin\SDK\Microsoft.Deployment.WindowsInstaller.dll;
//css_ref WixSharp.dll;
//css_ref WixSharp.UI.dll;

//css_inc .\Dialogs\*.cs
//css_res .\Dialogs\LicenceDialog.resx

using System;

using WixSharp;
using WixSharp.Forms;
using WixSharpSetup.Dialogs;

class Script 
{
    static public void Main(string[] args)
    {
        var pkgPath = Environment.GetEnvironmentVariable("DOSBOX_PKG_PATH");
        if (pkgPath.IsEmpty()) { Console.WriteLine("DOSBOX_PKG_PATH must be set!"); }

        var dosboxVers = Environment.GetEnvironmentVariable("DOSBOX_VERSION");
        if (dosboxVers.IsEmpty()) { Console.WriteLine("DOSBOX_VERSION must be set!"); }

        var outDir = Environment.GetEnvironmentVariable("DOSBOX_INSTALL_OUTDIR");
        var outFile = Environment.GetEnvironmentVariable("DOSBOX_INSTALL_OUTFILE");

        Console.WriteLine(string.Format("DOSBOX_PKG_PATH is '{0}'", pkgPath));
        Console.WriteLine(string.Format("DOSBOX_VERSION is '{0}'", dosboxVers));

        var project = new ManagedProject("Dosbox Staging",
                          new Dir(@"%ProgramFiles%\Dosbox-Staging\Dosbox Staging",
                              new Files(string.Format(@"{0}\*.*", pkgPath))));
 
        project.GUID = new Guid("18910652-7759-47c0-aaae-684ae15f18f1");

        project.LicenceFile = "gpl-2.0.rtf";

        project.Version = new Version(dosboxVers);
        project.Platform = Platform.x64;

        // Allow downgrades
        project.MajorUpgrade = new MajorUpgrade();
        project.MajorUpgrade.AllowDowngrades = true;

        project.ManagedUI = new ManagedUI();
        project.ManagedUI.InstallDialogs.Add(Dialogs.Welcome)
                                        .Add(Dialogs.InstallScope)
                                        .Add<LicenceDialog>()
                                        .Add(Dialogs.InstallDir)
                                        .Add(Dialogs.Progress)
                                        .Add(Dialogs.Exit);
        
        project.ManagedUI.ModifyDialogs.Add(Dialogs.MaintenanceType)
                                    .Add(Dialogs.Features)
                                    .Add(Dialogs.Progress)
                                    .Add(Dialogs.Exit);

        if (outDir.IsNotEmpty()) { project.OutDir = outDir; }
        if (outFile.IsNotEmpty()) { project.OutFileName = outFile; }

        Compiler.BuildMsi(project);

    }
} 
