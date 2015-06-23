import io.vertx.core.AbstractVerticle;
import io.vertx.core.json.JsonObject;
import io.vertx.ext.web.Router;
import io.vertx.ext.web.handler.sockjs.SockJSHandler;
import io.vertx.ext.web.handler.sockjs.BridgeOptions;
import io.vertx.ext.web.handler.sockjs.PermittedOptions;

public class Server extends AbstractVerticle {
	
	@Override
	public void start() throws Exception {
		Router router = Router.router(vertx);

		BridgeOptions bridge_opts = new BridgeOptions().addOutboundPermitted(new PermittedOptions().setAddress("test.pub"));
		router.route("/eventbus/*").handler(SockJSHandler.create(vertx).bridge(bridge_opts));

		vertx.createHttpServer().requestHandler(router::accept).listen(8080);

		vertx.setPeriodic(5000, tid -> {
			vertx.eventBus().publish("test.pub", new JsonObject().put("message", "Good news everyone!"));
		});
	}
}
