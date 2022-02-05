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
    public class ShellIntRegKey
    {
        string baseKey;
        bool isDir;
        Feature feat;
        RegistryHive hive = RegistryHive.LocalMachineOrUsers;
        public ShellIntRegKey(Feature feat, string baseKey, bool isDir = false)
        {
            this.feat = feat;
            this.baseKey = baseKey;
            this.isDir = isDir;
        }
        public RegKey getRootKey()
        {
            if (isDir)
            {
                return new RegKey(feat, hive, baseKey,
                                new RegValue("(default)", "Open with Dosbox Staging"),
                                new RegValue("Icon", @"""[INSTALLDIR]dosbox.exe"",0")
                           );
            } else
            {
                return new RegKey(feat, hive, baseKey, new RegValue("Icon", @"""[INSTALLDIR]dosbox.exe"",0"));
            }
        }

        public RegKey getCommandKey(string cmdOpt = "")
        {
            return new RegKey(feat, hive, baseKey,
                                new RegValue("(default)", 
                                    string.Format("\"[INSTALLDIR]dosbox.exe\"{0} \"%{1}\"", cmdOpt, isDir ? "v" : "i"))
                       );
        }
    }
    static public void Main(string[] args)
    {
        var pkgPath = Environment.GetEnvironmentVariable("DOSBOX_PKG_PATH");
        if (pkgPath.IsEmpty())
        {
            Console.WriteLine("DOSBOX_PKG_PATH must be set!");
            Environment.Exit(1);
        }

        var dosboxVers = Environment.GetEnvironmentVariable("DOSBOX_VERSION");
        if (dosboxVers.IsEmpty())
        {
            Console.WriteLine("DOSBOX_VERSION must be set!");
            Environment.Exit(1);
        }

        var outDir = Environment.GetEnvironmentVariable("DOSBOX_INSTALL_OUTDIR");
        var outFile = Environment.GetEnvironmentVariable("DOSBOX_INSTALL_OUTFILE");

        var dosboxArch = Environment.GetEnvironmentVariable("DOSBOX_INSTALL_ARCH");
        if (dosboxArch.IsEmpty()) { dosboxArch = "x64"; }
        if (dosboxArch != "x64" && dosboxArch != "x86")
        {
            Console.WriteLine("If set, DOSBOX_INSTALL_ARCH must be 'x86' or 'x64'");
            Environment.Exit(1);
        }

        Console.WriteLine(string.Format("DOSBOX_PKG_PATH is '{0}'", pkgPath));
        Console.WriteLine(string.Format("DOSBOX_VERSION is '{0}'", dosboxVers));

        var files = new Feature("Dosbox Files",
                                      "Files required to run Dosbox Staging",
                                      true,
                                      false);

        var shortcuts = new Feature("Shortcuts", "Start menu and desktop shortcuts");
        var shortcutsStart = new Feature("Start Menu");
        var shortcutsDesktop = new Feature("Desktop");
        shortcuts.Add(shortcutsStart);
        shortcuts.Add(shortcutsDesktop);

        var shellIntegration = new Feature("Context Menu", "Windows explorer context menu integration");
        var hive = RegistryHive.LocalMachineOrUsers;

        var shellKey = new ShellIntRegKey(shellIntegration, @"Software\Classes\Directory\shell\dosbox-staging", true);
        var shellBGKey = new ShellIntRegKey(shellIntegration, @"Software\Classes\Directory\Background\shell\dosbox-staging", true);
        var assocExe = new ShellIntRegKey(shellIntegration, @"Software\Classes\SystemFileAssociations\.exe\shell\Run with DOSBox Staging");
        var assocCom = new ShellIntRegKey(shellIntegration, @"Software\Classes\SystemFileAssociations\.com\shell\Run with DOSBox Staging");
        var assocBat = new ShellIntRegKey(shellIntegration, @"Software\Classes\SystemFileAssociations\.bat\shell\Run with DOSBox Staging");
        var assocConf = new ShellIntRegKey(shellIntegration, @"Software\Classes\SystemFileAssociations\.conf\shell\Open with DOSBox Staging");

        var project = new ManagedProject("Dosbox Staging",
                            new Dir(@"%ProgramFiles%\Dosbox Staging",
                                new Files(files, string.Format(@"{0}\*.*", pkgPath))
                            ),
                            new Dir(@"%ProgramMenu%\Dosbox Staging",
                                new ExeFileShortcut(shortcutsStart, "Dosbox Staging", "[INSTALLDIR]dosbox.exe", arguments: ""),
                                new ExeFileShortcut(shortcutsStart, "Uninstall Dosbox Staging", "[SystemFolder]msiexec.exe", "/x [ProductCode]")
                            ),
                            new Dir(@"%Desktop%",
                                new ExeFileShortcut(shortcutsDesktop, "Dosbox Staging", "[INSTALLDIR]dosbox.exe", arguments: "")
                            ),
                            shellKey.getRootKey(),
                            shellKey.getCommandKey(),
                            shellBGKey.getRootKey(),
                            shellBGKey.getCommandKey(),
                            assocExe.getRootKey(),
                            assocExe.getCommandKey(),
                            assocCom.getRootKey(),
                            assocCom.getCommandKey(),
                            assocBat.getRootKey(),
                            assocBat.getCommandKey(),
                            assocConf.getRootKey(),
                            assocConf.getCommandKey(" -conf")
                        );

        project.GUID = new Guid("18910652-7759-47c0-aaae-684ae15f18f1");

        project.LicenceFile = "gpl-2.0.rtf";

        project.Version = new Version(dosboxVers);
        project.Platform = dosboxArch == "x64" ? Platform.x64 : Platform.x86;

        // Allow downgrades
        project.MajorUpgrade = new MajorUpgrade();
        project.MajorUpgrade.AllowDowngrades = true;

        project.ManagedUI = new ManagedUI();
        project.ManagedUI.InstallDialogs.Add(Dialogs.Welcome)
                                        .Add(Dialogs.InstallScope)
                                        .Add<LicenceDialog>()
                                        .Add(Dialogs.SetupType)
                                        .Add(Dialogs.Features)
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
