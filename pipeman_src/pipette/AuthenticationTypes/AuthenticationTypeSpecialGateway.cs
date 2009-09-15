
using System;

namespace pipette
{

/// <summary>
/// NOT FOR GENERAL USE
/// </summary>
	public class AuthenticationTypeSpecialGateway : AuthenticationTypeAESLike
	{
		string xmlPrivateKey;
		public AuthenticationTypeSpecialGateway (string xmlPrivateKey): base(true)
		{
			this.xmlPrivateKey = xmlPrivateKey;
		}
		public override bool shouldAllowPerson (System.Net.IPEndPoint p)
		{
			//if (!base.shouldAllowPerson(p)) return false; you CAN trust a remote ip implicitly... this is a gateway
			logger.logger.info("Gateway: allowing connection from " + p.Serialize());
			return true;
		}
		public override void shouldVerifyIdentity (deepID d)
		{

			byte[] b = msgBase.read(d.plaintextStream,d);
			if (b.Length==0) d.raiseReadZeroBytes();
			logger.logger.info(xmlPrivateKey);
			byte[] decrypted = new byte[1];
			try
			{
				decrypted = cryptlib.RSA.decrypt(b,xmlPrivateKey);
			}
			catch (System.Security.Cryptography.CryptographicException e)
			{
				logger.logger.warn("Couldn't decrypt this data.  Pretending that the sender hung up.");
				d.raiseReadZeroBytes();
			}
			string plaintext = System.Text.Encoding.ASCII.GetString(decrypted);
			string[] parts = plaintext.Split('\n');
			string b64identity = parts[0];
			string b64key = parts[1];
			string dateReplayAttack = parts[2];
			DateTime rp = DateTime.Parse(dateReplayAttack);
			if ((DateTime.Now.ToUniversalTime() - rp).TotalSeconds > 60)
				throw new Exception("Replay attack detected!");
			string id = System.Text.Encoding.ASCII.GetString(Convert.FromBase64String(b64identity));
			d.aeskey = Convert.FromBase64String(b64key);
			
			logger.logger.info("Someone with id " + id + " connecting.");
			System.Net.WebClient c = new System.Net.WebClient();
			string result = c.DownloadString(magic.DIRECT_MOTHER_URI + "api/identity?identity=" + id);
			if (result != "YES")
				throw new VerificationFailedException();
			logger.logger.debug(result);
		}




	}
}
