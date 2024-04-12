using System;
using System.Diagnostics;
using System.Collections.Generic;
using Microsoft.Playfab.Gaming.GSDK.CSharp;

namespace FairlightGSDKWrapper
{
    class Program
    {
        private static Process gameProcess;

        static void Main(string[] args)
        {
            LogMessage("GSDK Wrapper for Fairlight Project");
            LogMessage("Initializing GSDK");
            InitializeGSDK();
            LogMessage("Starting Server Process");
            StartServer();
        }

        private static void StartServer()
        {
            gameProcess = StartProcess("/server/LyraServer.sh -log -nogsdk");
            gameProcess.OutputDataReceived += DataReceived;
            gameProcess.ErrorDataReceived += DataReceived;
            gameProcess.BeginOutputReadLine();
            gameProcess.BeginErrorReadLine();
            gameProcess.WaitForExit();
        }

        private static void InitializeGSDK()
        {
            GameserverSDK.RegisterShutdownCallback(OnShutdown);
            GameserverSDK.RegisterHealthCallback(OnHealthCheck);
            GameserverSDK.RegisterMaintenanceCallback(OnMaintenanceScheduled);
            GameserverSDK.Start();
        }

        private static Process StartProcess(string commandLine)
        {
            var escapedArgs = commandLine.Replace("\"", "\\\"");

            var serverProcess = new Process()
            {
                StartInfo = new ProcessStartInfo
                {
                    FileName = "/bin/bash",
                    Arguments = $"-c \"{escapedArgs}\"",
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true,
                }
            };

            serverProcess.Start();
            return serverProcess;
        }

        public static void DataReceived(object sender, DataReceivedEventArgs e)
        {
            Console.WriteLine(e.Data);

            if(e.Data.Contains("Engine is initialized"))
            {
                if(GameserverSDK.ReadyForPlayers())
                {
                    IDictionary<string, string> activeConfig = GameserverSDK.getConfigSettings();

                    if (activeConfig.TryGetValue(GameserverSDK.SessionCookieKey, out string sessionCookie))
                    {
                        LogMessage($"Got Session Cookie: {sessionCookie}");
                    }
                }
                else
                {
                    LogMessage("Server Terminated");
                    gameProcess?.Kill(); 
                }
            }
        }

        static void OnShutdown()
        {
            gameProcess?.Kill();
            Environment.Exit(0);
        }

        static bool OnHealthCheck()
        {
            return gameProcess != null;
        }

        static void OnMaintenanceScheduled(DateTimeOffset time)
        {
            LogMessage($"Maintenance Scheduled at: {time}");
        }

        private static void LogMessage(string message)
        {
            Console.WriteLine(message);
            GameserverSDK.LogMessage(message);
        }
    }
}
