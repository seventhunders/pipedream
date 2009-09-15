
using System;
using System.IO;
namespace pipette
{


	public class m0ther_api_request
	{

		public static string req (int port, string apiurl)
		{
			System.Net.WebRequest r = System.Net.HttpWebRequest.Create(magic.DIRECT_MOTHER_URI+apiurl);
			r.Proxy = new System.Net.WebProxy("http://127.0.0.1:"+port);
			StreamReader rs = new StreamReader(r.GetResponse().GetResponseStream());
			return rs.ReadToEnd();
		}
	}
}
