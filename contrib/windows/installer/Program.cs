using System;
using WixSharp;
using WixSharp.Forms;
using WixSharpSetup.Dialogs;

namespace installer
{
    class Program
    {
        static void Main()
        {
            var pkgPath = Environment.GetEnvironmentVariable("DOSBOX_PKG_PATH");
            if (pkgPath.IsEmpty()) { Console.WriteLine("DOSBOX_PKG_PATH must be set!"); }

            var dosboxVers = Environment.GetEnvironmentVariable("DOSBOX_VERSION");
            if (dosboxVers.IsEmpty()) { Console.WriteLine("DOSBOX_VERSION must be set!"); }

            var outDir = Environment.GetEnvironmentVariable("DOSBOX_INSTALL_OUTDIR");
            var outFile = Environment.GetEnvironmentVariable("DOSBOX_INSTALL_OUTFILE");

            Console.WriteLine($"DOSBOX_PKG_PATH is '{pkgPath}'");
            Console.WriteLine($"DOSBOX_VERSION is '{dosboxVers}'");

            var dosboxBin = new Feature("Dosbox Staging", "The Dosbox Staging binaries", true, false);
            var dosboxDebuggerBin = new Feature("Dosbox with Debugger", false, true);
            var translations = new Feature("Translation Files", true, false);
            var documentation = new Feature("Documentation", true, true);

            var project = new ManagedProject("Dosbox Staging",
                             new Dir(@"%ProgramFiles%\Dosbox-Staging\Dosbox Staging",
                                 new File(dosboxBin, $@"{pkgPath}\dosbox.exe"),
                                 new Files(dosboxBin, $@"{pkgPath}\COPYING.txt"),
                                 new DirFiles(dosboxBin, $@"{pkgPath}\*.dll"),
                                 new File(dosboxDebuggerBin, $@"{pkgPath}\dosbox_with_debugger.exe"),
                                 new Files(translations, $@"{pkgPath}\*.lng"),
                                 new Files(documentation, $@"{pkgPath}\*.txt", f => !f.Contains("COPYING"))));

            // Do not change this!
            project.GUID = new Guid("18910652-7759-47c0-aaae-684ae15f18f1");
            project.LicenceFile = "gpl-2.0.rtf";


            project.Version = new Version(dosboxVers);
            project.Platform = Platform.x64;

            //custom set of standard UI dialogs
            project.ManagedUI = new ManagedUI();

            project.ManagedUI.InstallDialogs.Add<WelcomeDialog>()
                                            .Add(Dialogs.InstallScope)
                                            .Add<LicenceDialog>()
                                            .Add<SetupTypeDialog>()
                                            .Add<FeaturesDialog>()
                                            .Add<InstallDirDialog>()
                                            .Add<ProgressDialog>()
                                            .Add<ExitDialog>();

            project.ManagedUI.ModifyDialogs.Add<MaintenanceTypeDialog>()
                                           .Add<FeaturesDialog>()
                                           .Add<ProgressDialog>()
                                           .Add<ExitDialog>();

            if (outDir.IsNotEmpty()) { project.OutDir = outDir; }
            if (outFile.IsNotEmpty()) { project.OutFileName = outFile; }

            ValidateAssemblyCompatibility();

            project.BuildMsi();
        }

        static void ValidateAssemblyCompatibility()
        {
            var assembly = System.Reflection.Assembly.GetExecutingAssembly();

            if (!assembly.ImageRuntimeVersion.StartsWith("v2."))
            {
                Console.WriteLine("Warning: assembly '{0}' is compiled for {1} runtime, which may not be compatible with the CLR version hosted by MSI. " +
                                  "The incompatibility is particularly possible for the EmbeddedUI scenarios. " +
                                   "The safest way to solve the problem is to compile the assembly for v3.5 Target Framework.",
                                   assembly.GetName().Name, assembly.ImageRuntimeVersion);
            }
        }
    }
}