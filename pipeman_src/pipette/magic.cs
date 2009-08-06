
using System;

namespace pipette
{


	public static class magic
	{
		/// <summary>
		/// AuthenticationTypeSpecialGateway writes directly to this port
		/// </summary>
		public static string DIRECT_MOTHER_URI="http://pipem0ther.appspot.com/";
		public static string DIRECT_MOTHER_HOST="pipem0ther.appspot.com";
		public static int DIRECT_MOTHER_PORT=80;
		
		public static int random_port()
		{
			Random r = new Random();
			return r.Next() % 64511 + 1024;
		}
	}
}
