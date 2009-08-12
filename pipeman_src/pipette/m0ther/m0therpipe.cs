
using System;

namespace pipette
{

	/// <summary>
	/// Special pipe that connects to m0ther
	/// </summary>
	public class m0therpipe : Pipe
	{

		public m0therpipe (int localBindPort,string gateway, int gatewayport, string identity,string m0therkey): base(   new PipeEndTypeLocalBind(System.Net.IPAddress.Loopback,localBindPort, new AuthenticationTypeNone(false)),
		new PipeEndTypeTCPClient(gateway,gatewayport, new AuthenticationTypeSpecialM0ther(identity,m0therkey)))
		{
			
		}
		
		public static m0therpipe bind_m0ther(out int port,string identity, string m0therkey)
		{
			//todo: run through this with a bunch of gateways
			logger.logger.push("mother-find");
			string GATEWAY_STR = "63.223.114.50";
			int GATEWAY_PORT = 12345;
			
			port = magic.random_port();
			m0therpipe p = new m0therpipe(port,GATEWAY_STR,GATEWAY_PORT,identity,m0therkey);
			p.start();
			//System.Net.ServicePointManager.ServerCertificateValidationCallback = new System.Net.Security.RemoteCertificateValidationCallback(AuthenticationTypeServiceAccept.BypassSslCertificateValidation);
			try
			{
				string result = m0ther_api_request.req(port,"/api/areyouthere");
				if (result=="YES")
				{
					logger.logger.debug("m0ther found; real request to follow");
					logger.logger.pop();
					return p;
				}
			}
			catch (Exception ex)
			{
				//move on to the next one
				throw ex;
			}
			throw new Exception("Cannot find m0ther");
			
			
		}
	}
}
