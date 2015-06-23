import io.vertx.core.AbstractVerticle;
import io.vertx.core.AsyncResult;
import io.vertx.core.Handler;
import io.vertx.core.eventbus.Message;
import io.vertx.core.json.JsonObject;
import io.vertx.ext.web.Router;
import io.vertx.ext.web.handler.sockjs.SockJSHandler;
import io.vertx.ext.web.handler.sockjs.BridgeOptions;
import io.vertx.ext.web.handler.sockjs.PermittedOptions;

public class Server extends AbstractVerticle {
	
	@Override
	public void start() throws Exception {
		Router router = Router.router(vertx);

		BridgeOptions bridge_opts = new BridgeOptions().addInboundPermitted(new PermittedOptions().setAddress("test.echo"));
		router.route("/eventbus/*").handler(SockJSHandler.create(vertx).bridge(bridge_opts));

		vertx.createHttpServer().requestHandler(router::accept).listen(8080);

		vertx.eventBus().consumer("test.echo", new Handler<Message<JsonObject>>() {
			public void handle(Message<JsonObject> message) {
				reply(message);
			}
		});
	}
	
	private void reply(Message<JsonObject> message) {
		JsonObject jmsg = message.body();
				
		if(jmsg != null && jmsg.containsKey("message"))
		{
			// Echo back the message
			message.reply(new JsonObject().put("message", jmsg.getString("message")), (AsyncResult<Message<JsonObject>> as) -> {
				if(as.succeeded())
				{
					reply(as.result());
				}
			});
		}
		else
		{
			message.reply(new JsonObject().put("message", "Bye!"));
		}
	}
}
